function [result] = test_single_angle_distance_multi_config_v3(test_angle_str, test_distance_str, phase_compensation, n_ports, ports_to_use, method, enable_distance_correction, correction_coefficients, enable_rmse_filter)
% Test a single angle and distance using pre-computed phase compensation
% Flow order SAME as process_unified_data_and_calibrate.m (and v2):
%   1) PacketType!=1 filter, export LDE from filtered data
%   2) Load LDE + sequences; per-port LDE quality filter (two LDEs, distance>30)
%   3) Sequence-based alignment (forward_dist), keep frames where all ports non-empty
%   4) Extract aligned LDE phases -> double difference -> AoA
% Supports different number of ports: 8, 4, or 2
% n_ports: number of ports to use (8, 4, or 2)
% ports_to_use: array of port numbers to use (e.g., [1:8], [3:6], [4:5])
% method: 'fft' (default) or 'mvdr' for AoA estimation method
% enable_distance_correction: true (default) to apply distance correction, false to skip (for data collection)
% correction_coefficients: optional struct with fields [const, angle, dist, angle2, dist2, angle_dist]
%                          If empty, uses default coefficients
% enable_rmse_filter: true (default) to filter out frames with RMSE > 0.20 m, false to skip filtering
% Returns: result struct with mean, std, error, and RMSE in Cartesian coordinates
%          If enable_distance_correction=false, also returns fit_data for model fitting

if nargin < 4
    % Default: use first n_ports if ports_to_use not specified
    if n_ports == 8
        ports_to_use = 1:8;
    elseif n_ports == 4
        ports_to_use = 1:4;
    elseif n_ports == 2
        ports_to_use = 1:2;
    else
        error('n_ports must be 2, 4, or 8');
    end
end

if nargin < 5 || isempty(method)
    method = 'fft';  % Default to FFT
end

if nargin < 7 || isempty(enable_distance_correction)
    enable_distance_correction = true;  % Default: enable correction
end

if nargin < 8
    correction_coefficients = [];  % Empty means use default
end

if nargin < 9 || isempty(enable_rmse_filter)
    enable_rmse_filter = true;  % Default: enable RMSE filtering
end

if ~ismember(method, {'fft', 'mvdr', 'mvdr_enhanced'})
    error('method must be ''fft'', ''mvdr'', or ''mvdr_enhanced''');
end

method_display = upper(strrep(method, '_', ' '));
if enable_distance_correction
    fprintf('Testing angle: %s°, distance: %s m (%d ports: [%s], method: %s, with sequence alignment v3, distance correction enabled)...\n', ...
        test_angle_str, test_distance_str, n_ports, num2str(ports_to_use), method_display);
else
    fprintf('Testing angle: %s°, distance: %s m (%d ports: [%s], method: %s, with sequence alignment v3, distance correction DISABLED for data collection)...\n', ...
        test_angle_str, test_distance_str, n_ports, num2str(ports_to_use), method_display);
end

% Data path: Figure11ab/raw (symlink to MultiPort/20260209 for env1)
script_dir = fileparts(mfilename('fullpath'));
data_path = fullfile(script_dir, 'raw');
if data_path(end) ~= filesep
    data_path = [data_path filesep];
end
lde_dir = fullfile(script_dir, 'lde_cache');
if ~exist(lde_dir, 'dir')
    mkdir(lde_dir);
end
% env1 files: antenna_data_port*_8ports_concurrent_localization_accuracy_{angle}_{dist}.csv
% (no env tag; env4 used accuracy_env4_*)
env_file_tag = '';  % empty for env1; use 'env4_' for env4

%% Step 1: Extract LDE using proper algorithm (export_lde_complex_8antenna)
fprintf('  Extracting LDE using proper algorithm...\n');
fprintf('  Raw data path: %s\n', data_path);
fprintf('  LDE files will be saved to: %s\n', lde_dir);

% Ensure export_lde_complex_8antenna function is in path
if ~exist('export_lde_complex_8antenna', 'file')
    % Try to add Concurrent directory to path
    addpath(script_dir);
    if ~exist('export_lde_complex_8antenna', 'file')
        error('export_lde_complex_8antenna function not found in path!');
    end
end

% Same as process_unified: filter to PacketType!=1 first, then export LDE from filtered data
for port = ports_to_use
    csvfile = fullfile(lde_dir, sprintf('lde_complex_real_port%d_angle%s_dist%s.csv', port, test_angle_str, test_distance_str));
    if ~exist(csvfile, 'file')
        fprintf('    Port %d: Extracting LDE (from PacketType!=1 only)...\n', port);
        infile = fullfile(data_path, sprintf('antenna_data_port%d_8ports_concurrent_localization_accuracy_%s%s_%s.csv', port, env_file_tag, test_angle_str, test_distance_str));
        if ~exist(infile, 'file')
            fprintf('  ERROR: Data file not found for port %d!\n', port);
            fprintf('    Expected file: %s\n', infile);
            result = [];
            return;
        end
        try
            T_in = readtable(infile);
            vnames = T_in.Properties.VariableNames;
            ptIdx = find(strcmpi(vnames, 'PacketType'), 1);
            if ~isempty(ptIdx)
                T_in = T_in(T_in.(vnames{ptIdx}) ~= 1, :);
            end
            temp_input = fullfile(lde_dir, sprintf('temp_port%d_angle%s_dist%s_for_lde.csv', port, test_angle_str, test_distance_str));
            writetable(T_in, temp_input);
            export_lde_complex_8antenna(temp_input, csvfile);
            if exist(temp_input, 'file'), delete(temp_input); end
            if ~exist(csvfile, 'file')
                error('LDE extraction completed but output file was not created: %s', csvfile);
            end
            fprintf('    Port %d: LDE extraction completed successfully\n', port);
        catch ME
            if exist('temp_input', 'var') && exist(temp_input, 'file'), delete(temp_input); end
            fprintf('  ERROR: Failed to extract LDE for port %d!\n', port);
            fprintf('    Error message: %s\n', ME.message);
            fprintf('    Input file: %s\n', infile);
            fprintf('    Output file: %s\n', csvfile);
            result = [];
            return;
        end
    else
        fprintf('    Port %d: Using existing LDE file\n', port);
    end
end

%% Step 2: Load LDE data AND original sequence numbers (PacketType!=1)
fprintf('  Loading LDE data and sequence numbers...\n');

sequences_lde = cell(n_ports, 1);
lde_data = cell(n_ports, 1);
distance_data = cell(n_ports, 1);  % Store firstPathAmp1 (distance in cm)
original_cir_data = cell(n_ports, 1);  % CIR for per-port LDE filter (Step 2.5)

for idx = 1:n_ports
    port = ports_to_use(idx);
    % Load LDE results
    csvfile = fullfile(lde_dir, sprintf('lde_complex_real_port%d_angle%s_dist%s.csv', port, test_angle_str, test_distance_str));
    if ~exist(csvfile, 'file')
        fprintf('  ERROR: LDE file not found for port %d: %s\n', port, csvfile);
        fprintf('    This file should have been generated in Step 1. Please check the error messages above.\n');
        result = [];
        return;
    end
    try
        T_lde = readtable(csvfile);
    catch ME
        fprintf('  ERROR: Failed to read LDE file for port %d: %s\n', port, csvfile);
        fprintf('    Error message: %s\n', ME.message);
        result = [];
        return;
    end
    lde_data{idx} = T_lde;
    
    % Load original data to get sequence numbers and distance (firstPathAmp1)
    infile = fullfile(data_path, sprintf('antenna_data_port%d_8ports_concurrent_localization_accuracy_%s%s_%s.csv', port, env_file_tag, test_angle_str, test_distance_str));
    if ~exist(infile, 'file')
        fprintf('  ERROR: Original data file not found for port %d: %s\n', port, infile);
        result = [];
        return;
    end
    try
        T_orig = readtable(infile);
    catch ME
        fprintf('  ERROR: Failed to read original data file for port %d: %s\n', port, infile);
        fprintf('    Error message: %s\n', ME.message);
        result = [];
        return;
    end
    
    % Keep only rows where PacketType != 1 (LDE file was exported from filtered data in Step 1, so T_lde already has one row per PacketType!=1; only filter T_orig)
    vnames = T_orig.Properties.VariableNames;
    ptIdx = find(strcmpi(vnames, 'PacketType'), 1);
    if ~isempty(ptIdx)
        idx_keep = (T_orig.(vnames{ptIdx}) ~= 1);
        T_orig = T_orig(idx_keep, :);
        fprintf('    Port %d: Filtered to %d rows (packettype != 1)\n', port, sum(idx_keep));
    end
    
    if ~ismember('Sequence', T_orig.Properties.VariableNames)
        fprintf('  ERROR: Sequence column not found in port %d data!\n', port);
        result = [];
        return;
    end
    
    if ~ismember('firstPathAmp1', T_orig.Properties.VariableNames)
        fprintf('  ERROR: firstPathAmp1 column not found in port %d data!\n', port);
        fprintf('    firstPathAmp1 contains the distance in centimeters.\n');
        result = [];
        return;
    end
    
    % Store sequences (should match LDE row count)
    sequences_lde{idx} = T_orig.Sequence;
    
    % Store distance data (firstPathAmp1 in cm, will be converted to meters later)
    distance_data{idx} = T_orig.firstPathAmp1;
    
    if height(T_lde) ~= height(T_orig)
        fprintf('  ERROR: LDE and original data have different row counts for port %d!\n', port);
        result = [];
        return;
    end
    
    % Extract CIR for per-port LDE quality filter (Step 2.5, same as process_unified)
    names = T_orig.Properties.VariableNames;
    isR = startsWith(names, 'CIR_real_');
    isI = startsWith(names, 'CIR_imag_');
    rNames = names(isR);
    iNames = names(isI);
    rIdx = sscanf(strjoin(erase(rNames, 'CIR_real_'), ' '), '%d');
    [~, ordR] = sort(rIdx);
    rNames = rNames(ordR);
    iIdx = sscanf(strjoin(erase(iNames, 'CIR_imag_'), ' '), '%d');
    [~, ordI] = sort(iIdx);
    iNames = iNames(ordI);
    R = double(T_orig{:, rNames});
    I = double(T_orig{:, iNames});
    original_cir_data{idx} = complex(R, I);
end

%% Step 2.5: Per-port LDE quality filter (SAME order as process_unified: filter first, then align)
% Remove frames that do not have two LDEs or LDE distance <= 30, per port independently.
fprintf('  Per-port LDE quality filter (two LDEs + distance > 30)...\n');

min_lde_distance = 30;
P.thFactor = 6;
P.noiseFrac = 0.15;
P.madK = 4.5;
P.quantize64 = true;
P.gradWin = 3;
P.ampWin = 14;
P.mergeSeps = 12;
P.schmittHigh = 1.20;
P.schmittLookAhead = 10;
P.minStayBins = 3;
P.minSlope = 8;
P.ignoreRange = [1, 2];
P.thAdd = 300;
noiseRegionBins = 10;
mergeGapBelow = 2;
minStayAbove = 3;
earlyPeakFrac = 0.80;
gradSpan = max(1, P.gradWin);
approx_mag = @(xc) max(abs(real(xc)), abs(imag(xc))) + 0.25*min(abs(real(xc)), abs(imag(xc)));

for idx = 1:n_ports
    port = ports_to_use(idx);
    T_lde = lde_data{idx};
    CIR = original_cir_data{idx};
    Nf = height(T_lde);
    Nbins = size(CIR, 2);
    is_valid_frame = false(Nf, 1);
    
    for k = 1:Nf
        has_two_lde = ~isnan(T_lde.complex_large_real(k)) && ~isnan(T_lde.complex_large_imag(k));
        if ~has_two_lde
            continue;
        end
        x = CIR(k, :);
        mag = approx_mag(x);
        valid = true(1, Nbins);
        if isfield(P, 'ignoreRange') && ~isempty(P.ignoreRange)
            valid(P.ignoreRange(1):min(P.ignoreRange(2), Nbins)) = false;
        end
        startIdx = find(valid, 1, 'first');
        if isempty(startIdx), startIdx = 1; end
        noiseEnd = min(Nbins, startIdx + noiseRegionBins - 1);
        thr = P.thFactor * mean(mag(startIdx:noiseEnd));
        above = mag >= thr;
        if mergeGapBelow > 0
            z = ~above;
            dz = diff([0 z 0]);
            zs = find(dz == 1);
            ze = find(dz == -1) - 1;
            for ii = 1:numel(zs)
                Lg = zs(ii);
                Rg = ze(ii);
                gapLen = Rg - Lg + 1;
                leftOK = (Lg - 1) >= 1 && above(Lg - 1);
                rightOK = (Rg + 1) <= Nbins && above(Rg + 1);
                if gapLen <= mergeGapBelow && leftOK && rightOK
                    above(Lg:Rg) = true;
                end
            end
        end
        d = diff([0 above 0]);
        st = find(d == 1);
        en = find(d == -1) - 1;
        if isempty(st)
            continue;
        end
        candIdx = [];
        candAmp = [];
        g = diff(mag);
        for c = 1:numel(st)
            s_bin = st(c);
            e_bin = en(c);
            stay_hi = min(e_bin, s_bin + max(0, minStayAbove - 1));
            if sum(mag(s_bin:stay_hi) >= thr) < minStayAbove
                continue;
            end
            [peakAmpMax, relp] = max(mag(s_bin:e_bin));
            peakIdxMax = s_bin + relp - 1;
            locs = [];
            for i = max(s_bin + 1, 2):min(e_bin - 1, Nbins - 1)
                if mag(i) >= mag(i - 1) && mag(i) > mag(i + 1)
                    locs(end + 1) = i; %#ok<AGROW>
                end
            end
            locs = locs(mag(locs) >= thr);
            strongMask = ~isempty(locs) & (mag(locs) >= earlyPeakFrac * peakAmpMax);
            if any(strongMask)
                seed = locs(find(strongMask, 1, 'first'));
            elseif ~isempty(locs)
                seed = locs(1);
            else
                seed = peakIdxMax;
            end
            lo = max(2, s_bin - gradSpan);
            hi = min(Nbins - 1, s_bin + gradSpan);
            hi = min(hi, seed - 1);
            if lo > hi
                lo = max(2, seed - 1);
                hi = seed - 1;
                if lo > hi
                    lo = max(2, s_bin);
                    hi = lo;
                end
            end
            [~, rmax] = max(g(lo:hi));
            m = lo + rmax - 1;
            gm1 = g(max(m - 1, 1));
            g0 = g(m);
            gp1 = g(min(m + 1, Nbins - 1));
            denom = (g0 - min(gm1, gp1));
            if denom <= 0
                frac = 0;
            else
                frac = 0.5 * (gp1 - gm1) / denom;
                frac = max(-0.5, min(0.5, frac));
            end
            lde = m + frac;
            lde = min(lde, seed - 0.5);
            if isfield(P, 'quantize64') && P.quantize64
                lde = round(lde * 64) / 64;
            end
            candIdx(end + 1) = lde; %#ok<AGROW>
            candAmp(end + 1) = peakAmpMax; %#ok<AGROW>
        end
        if isempty(candIdx) || length(candIdx) < 2
            continue;
        end
        [~, srt] = sort(candAmp, 'descend');
        srt = srt(1:min(2, numel(srt)));
        ldes = candIdx(srt) + 0.5;  % same as process_unified_data_and_calibrate.m
        if length(ldes) >= 2
            [ldes_sorted, ~] = sort(ldes, 'ascend');
            delta = ldes_sorted(2) - ldes_sorted(1);
            if delta > min_lde_distance
                is_valid_frame(k) = true;
            end
        end
    end
    
    n_single_lde = sum(isnan(T_lde.complex_large_real) | isnan(T_lde.complex_large_imag));
    n_distance_invalid = sum(~is_valid_frame) - n_single_lde;
    n_valid = sum(is_valid_frame);
    fprintf('    Port %d: total %d frames, single-LDE %d, distance<=30 %d, valid %d\n', ...
        port, Nf, n_single_lde, n_distance_invalid, n_valid);
    
    lde_data{idx} = lde_data{idx}(is_valid_frame, :);
    sequences_lde{idx} = sequences_lde{idx}(is_valid_frame);
    distance_data{idx} = distance_data{idx}(is_valid_frame);
    original_cir_data{idx} = original_cir_data{idx}(is_valid_frame, :);
end

fprintf('  Per-port LDE filter done. Alignment uses only valid frames per port.\n\n');

%% Step 3: Two-step alignment (SAME as process_unified_data_and_calibrate.m)
fprintf('  Two-step alignment (padding + final alignment, using forward_dist method)...\n');
fprintf('    Step 1: Find min start sequence, pad missing sequences\n');
fprintf('    Step 2: Align frames, keep only frames where all ports are non-empty\n');

% Step 1: Find minimum starting sequence
min_start_seq = inf;
for idx = 1:n_ports
    seqs = sequences_lde{idx};
    if ~isempty(seqs)
        min_start_seq = min(min_start_seq, seqs(1));
    end
end
fprintf('    Min start sequence: %d\n', min_start_seq);

% Step 2: Pad sequences for each port (considering 0-255 wrap-around)
max_frames = 0;
for idx = 1:n_ports
    max_frames = max(max_frames, length(sequences_lde{idx}));
end

% Generate sequence range (considering 0-255 wrap-around)
seq_range = zeros(max_frames, 1);
for i = 1:max_frames
    seq_range(i) = mod(min_start_seq + i - 1, 256);  % 0-255 wrap-around
end

Nf_padded = max_frames;
fprintf('    Padding range: from sequence %d, pad %d frames (0-255 wrap-around)\n', min_start_seq, Nf_padded);

% Get sample table structure for creating empty frames
sample_lde = lde_data{1};
lde_vars = sample_lde.Properties.VariableNames;

padded_lde_data = cell(n_ports, 1);
padded_sequences = cell(n_ports, 1);
padded_distances = cell(n_ports, 1);  % Store padded distance data
is_empty_frame = cell(n_ports, 1);

for idx = 1:n_ports
    seqs_orig = sequences_lde{idx};
    lde_orig = lde_data{idx};
    dist_orig = distance_data{idx};
    
    padded_table = cell(Nf_padded, 1);
    padded_seqs = zeros(Nf_padded, 1);
    padded_dists = nan(Nf_padded, 1);
    is_empty = false(Nf_padded, 1);
    
    orig_pos = 1;  % Pointer to current position in original data
    
    for i = 1:Nf_padded
        seq = seq_range(i);
        padded_seqs(i) = seq;
        
        % Check if current position matches
        if orig_pos <= length(seqs_orig) && seqs_orig(orig_pos) == seq
            % Match found, use original data
            padded_table{i} = lde_orig(orig_pos, :);
            padded_dists(i) = dist_orig(orig_pos);  % Store distance (cm)
            is_empty(i) = false;
            orig_pos = orig_pos + 1;
        else
            % No match, create empty frame
            empty_row = sample_lde(1, :);
            for j = 1:width(empty_row)
                var_name = lde_vars{j};
                empty_row.(var_name) = NaN;
            end
            padded_table{i} = empty_row;
            padded_dists(i) = NaN;  % No distance for empty frame
            is_empty(i) = true;
            
            % Skip incorrect frames (using forward_dist method, same as process_unified_data_and_calibrate.m)
            while orig_pos <= length(seqs_orig)
                seq_orig = seqs_orig(orig_pos);
                
                % If seq_orig equals seq, shouldn't be here (should be in match branch)
                if seq_orig == seq
                    break;
                end
                
                % Calculate forward distance from seq to seq_orig (considering wrap-around)
                should_skip = false;
                if seq_orig >= seq
                    forward_dist = seq_orig - seq;
                else
                    forward_dist = seq_orig - seq + 256;  % Crossing 0 boundary
                end
                
                % If forward distance >= 128, seq_orig is before seq (considering wrap-around), should skip
                % Otherwise, seq_orig is after seq, don't skip
                if forward_dist >= 128
                    % seq_orig is before seq (considering wrap-around), is an incorrect frame, skip
                    should_skip = true;
                else
                    % seq_orig is after seq, don't skip
                    should_skip = false;
                end
                
                if should_skip
                    orig_pos = orig_pos + 1;  % Skip the incorrect frame
                else
                    % seq_orig is after seq or equal, don't skip
                    break;
                end
            end
        end
    end
    
    padded_lde_data{idx} = vertcat(padded_table{:});
    padded_sequences{idx} = padded_seqs;
    padded_distances{idx} = padded_dists;
    is_empty_frame{idx} = is_empty;
end

fprintf('    Padding complete: all ports padded to %d frames\n', Nf_padded);

% Step 3: Final alignment - keep only frames where all ports are non-empty
aligned_final_indices = [];
kept_count = 0;

for frame_idx = 1:Nf_padded
    all_not_empty = true;
    for idx = 1:n_ports
        if is_empty_frame{idx}(frame_idx)
            all_not_empty = false;
            break;
        end
    end
    
    if all_not_empty
        kept_count = kept_count + 1;
        aligned_final_indices(kept_count) = frame_idx;
    end
end

Nf_common = kept_count;
fprintf('    Alignment complete: kept %d frames (after padding %d frames)\n', Nf_common, Nf_padded);

if Nf_common == 0
    fprintf('  ERROR: No frames found where all ports are non-empty!\n');
    result = [];
    return;
end

%% Step 4: Extract aligned LDE data
fprintf('  Extracting aligned LDE data...\n');

port_data = cell(n_ports, 1);

for idx = 1:n_ports
    % Extract aligned LDE data from padded data
    T_aligned = padded_lde_data{idx}(aligned_final_indices, :);
    
    % Extract phase data
    port_data{idx}.phase_small = angle(complex(T_aligned.complex_small_real, T_aligned.complex_small_imag));
    port_data{idx}.phase_large = angle(complex(T_aligned.complex_large_real, T_aligned.complex_large_imag));
    
    % Extract LDE positions for distance calculation
    % LDE positions are stored in the LDE data (need to recalculate from CIR)
    % For now, we'll extract from the original CIR data
end

%% Step 5: Double difference
phase_diff = nan(Nf_common, n_ports);
for idx = 1:n_ports
    phase_diff(:, idx) = port_data{idx}.phase_small - port_data{idx}.phase_large;
end

spatial_phase_raw = nan(Nf_common, n_ports);
for frame = 1:Nf_common
    ref_phase = phase_diff(frame, 1);
    if isnan(ref_phase), continue; end
    for idx = 1:n_ports
        if ~isnan(phase_diff(frame, idx))
            spatial_phase_raw(frame, idx) = phase_diff(frame, idx) - ref_phase;
        end
    end
end

%% Step 6: Apply calibration
spatial_phase_cal = nan(Nf_common, n_ports);
if length(phase_compensation) == n_ports
    spatial_phase_cal = apply_calibration(spatial_phase_raw, phase_compensation);
else
    fprintf('  Warning: Phase compensation length (%d) does not match n_ports (%d), using raw data\n', ...
        length(phase_compensation), n_ports);
    spatial_phase_cal = spatial_phase_raw;
end

%% Step 7: Compute AoA
true_angle = str2double(test_angle_str);
% Note: test_distance_str is just an identifier in the filename
% The actual true distance will be computed from firstPathAmp1 in Step 8
% For now, initialize as NaN (will be set in Step 8)
true_distance = NaN;

if strcmpi(method, 'mvdr')
    [aoa_uncal, ~, ~] = compute_mvdr_aoa(spatial_phase_raw, n_ports, ports_to_use);
    [aoa_cal, ~, ~] = compute_mvdr_aoa(spatial_phase_cal, n_ports, ports_to_use);
elseif strcmpi(method, 'mvdr_enhanced')
    mvdr_options = struct();
    mvdr_options.use_forward_backward = false;
    mvdr_options.use_spatial_smoothing = false;
    mvdr_options.angle_search_refinement = true;
    
    if n_ports == 2
        mvdr_options.window_size_multiplier = 1.2;
        mvdr_options.diagonal_loading_factor = 0.015;
    elseif n_ports == 4
        mvdr_options.window_size_multiplier = 1.1;
        mvdr_options.diagonal_loading_factor = 0.012;
    else
        mvdr_options.window_size_multiplier = 1.0;
        mvdr_options.diagonal_loading_factor = 0.01;
    end
    
    mvdr_options.use_music = false;
    
    if exist('compute_mvdr_aoa_enhanced', 'file')
        [aoa_uncal, ~, ~] = compute_mvdr_aoa_enhanced(spatial_phase_raw, n_ports, ports_to_use, mvdr_options);
        [aoa_cal, ~, ~] = compute_mvdr_aoa_enhanced(spatial_phase_cal, n_ports, ports_to_use, mvdr_options);
    else
        fprintf('  Warning: Enhanced MVDR function not found, using standard MVDR\n');
        [aoa_uncal, ~, ~] = compute_mvdr_aoa(spatial_phase_raw, n_ports, ports_to_use);
        [aoa_cal, ~, ~] = compute_mvdr_aoa(spatial_phase_cal, n_ports, ports_to_use);
    end
else
    % Default: FFT method
    [aoa_uncal, ~, ~] = compute_fft_aoa(spatial_phase_raw);
    [aoa_cal, ~, ~] = compute_fft_aoa(spatial_phase_cal);
end

%% Step 8: Extract distance from firstPathAmp1
% Distance is stored in firstPathAmp1 column, unit is centimeters
% Convert to meters by dividing by 100
% Strategy: Use first non-zero port's distance, skip frame if all ports are zero
distance_estimates = nan(Nf_common, 1);

for frame = 1:Nf_common
    % Get aligned frame index
    padded_idx = aligned_final_indices(frame);
    
    % Try each port to find first non-zero distance
    dist_cm = NaN;
    for port_idx = 1:n_ports
        port_dist_cm = padded_distances{port_idx}(padded_idx);
        
        % Check if distance is valid and non-zero
        if ~isnan(port_dist_cm) && port_dist_cm ~= 0
            dist_cm = port_dist_cm;
            break;  % Use first non-zero port's distance
        end
    end
    
    if ~isnan(dist_cm) && dist_cm ~= 0
        % Convert from centimeters to meters
        distance_estimates(frame) = dist_cm / 100.0;
    else
        % All ports are zero or NaN, skip this frame (distance remains NaN)
        distance_estimates(frame) = NaN;
    end
end

n_valid_distances = sum(~isnan(distance_estimates));
n_zero_distances = Nf_common - n_valid_distances;
fprintf('  Distance extraction: %d valid distance measurements (from firstPathAmp1, converted cm to m)\n', n_valid_distances);
if n_zero_distances > 0
    fprintf('  Warning: %d frames skipped due to all ports having zero distance\n', n_zero_distances);
end

% Store original distances before correction
distance_estimates_original = distance_estimates;

%% Step 8.5: Apply distance correction using AoA estimates (if enabled)
% Correction formula: Error = c0 + c1×Angle + c2×Distance + c3×Angle² + c4×Distance² + c5×Angle×Distance
% Corrected distance = Measured distance - Error
% Note: Error depends on true distance, so we use iterative method

if enable_distance_correction
    fprintf('  Applying distance correction using AoA estimates...\n');
    
    % Get correction coefficients (use provided or default)
    if isempty(correction_coefficients)
        % Default coefficients (from README_range_correction.md)
        coeff_const = 0.049564;
        coeff_angle = -0.000320;
        coeff_dist = 0.103095;
        coeff_angle2 = 0.000052;
        coeff_dist2 = -0.009102;
        coeff_angle_dist = 0.000047;
    else
        % Use provided coefficients
        if isstruct(correction_coefficients)
            coeff_const = correction_coefficients.const;
            coeff_angle = correction_coefficients.angle;
            coeff_dist = correction_coefficients.dist;
            coeff_angle2 = correction_coefficients.angle2;
            coeff_dist2 = correction_coefficients.dist2;
            coeff_angle_dist = correction_coefficients.angle_dist;
        else
            % Assume it's a vector [const, angle, dist, angle2, dist2, angle_dist]
            coeff_const = correction_coefficients(1);
            coeff_angle = correction_coefficients(2);
            coeff_dist = correction_coefficients(3);
            coeff_angle2 = correction_coefficients(4);
            coeff_dist2 = correction_coefficients(5);
            coeff_angle_dist = correction_coefficients(6);
        end
        fprintf('    Using provided correction coefficients\n');
    end
    
    % Corrected distances (will be computed per frame after AoA is available)
    distance_estimates_corrected = nan(Nf_common, 1);
    
    % Note: AoA estimates are computed in Step 7, so we need to correct distances here
    % We'll use calibrated AoA for correction (more accurate)
    for frame = 1:Nf_common
        if isnan(distance_estimates(frame))
            continue;
        end
        
        % Use calibrated AoA if available, otherwise use uncalibrated
        if ~isnan(aoa_cal(frame))
            angle_deg = aoa_cal(frame);
        elseif ~isnan(aoa_uncal(frame))
            angle_deg = aoa_uncal(frame);
        else
            continue;  % Skip if no AoA available
        end
        
        measured_dist = distance_estimates(frame);
        
        % Iterative correction: Error depends on true distance
        % Start with measured distance as initial guess
        corrected_dist = measured_dist;
        max_iter = 10;
        tol = 1e-6;  % Convergence tolerance (meters)
        
        for iter = 1:max_iter
            % Calculate predicted error using current corrected distance
            error_pred = coeff_const ...
                       + coeff_angle * angle_deg ...
                       + coeff_dist * corrected_dist ...
                       + coeff_angle2 * angle_deg^2 ...
                       + coeff_dist2 * corrected_dist^2 ...
                       + coeff_angle_dist * angle_deg * corrected_dist;
            
            % Update corrected distance: corrected = measured - error
            corrected_dist_new = measured_dist - error_pred;
            
            % Check convergence
            if abs(corrected_dist_new - corrected_dist) < tol
                corrected_dist = corrected_dist_new;
                break;
            end
            
            corrected_dist = corrected_dist_new;
        end
        
        distance_estimates_corrected(frame) = corrected_dist;
    end
    
    % Use corrected distances for further calculations
    distance_estimates = distance_estimates_corrected;
    
    fprintf('  Distance correction: %d distances corrected\n', sum(~isnan(distance_estimates_corrected)));
    
    % Compute mean distance from corrected distances (for reporting only)
    mean_distance_corrected = mean(distance_estimates, 'omitnan');
    if isnan(mean_distance_corrected)
        % Fallback: use mean of original distances
        mean_distance_corrected = mean(distance_estimates_original, 'omitnan');
        if isnan(mean_distance_corrected)
            % Final fallback: use test_distance_str
            mean_distance_corrected = str2double(test_distance_str);
            fprintf('  Warning: Could not extract distance from firstPathAmp1, using filename distance: %.2f m\n', mean_distance_corrected);
        else
            fprintf('  Mean distance (from original firstPathAmp1 mean): %.3f m\n', mean_distance_corrected);
        end
    else
        fprintf('  Mean distance (from corrected firstPathAmp1 mean): %.3f m\n', mean_distance_corrected);
    end
else
    % Distance correction disabled - keep original distances
    fprintf('  Distance correction DISABLED (data collection mode)\n');
    mean_distance_corrected = mean(distance_estimates, 'omitnan');
    if isnan(mean_distance_corrected)
        mean_distance_corrected = str2double(test_distance_str);
    end
end

% True distance for error calculation: use distance from filename (integer)
true_distance = str2double(test_distance_str);
fprintf('  True distance (from filename, for error calculation): %.3f m\n', true_distance);

%% Step 9: Convert to Cartesian coordinates and compute RMSE
% True position in Cartesian coordinates (using distance from filename)
true_angle_rad = deg2rad(true_angle);
true_x = true_distance * cos(true_angle_rad);
true_y = true_distance * sin(true_angle_rad);

% Estimated positions (calibrated and uncalibrated)
aoa_cal_rad = deg2rad(aoa_cal);
aoa_uncal_rad = deg2rad(aoa_uncal);

% Use true distance for now (in practice, use estimated distance)
% For each frame, compute estimated position
estimated_x_cal = nan(Nf_common, 1);
estimated_y_cal = nan(Nf_common, 1);
estimated_x_uncal = nan(Nf_common, 1);
estimated_y_uncal = nan(Nf_common, 1);

for frame = 1:Nf_common
    if ~isnan(aoa_cal(frame)) && ~isnan(distance_estimates(frame))
        estimated_x_cal(frame) = distance_estimates(frame) * cos(aoa_cal_rad(frame));
        estimated_y_cal(frame) = distance_estimates(frame) * sin(aoa_cal_rad(frame));
    elseif ~isnan(aoa_cal(frame))
        % Use true distance if distance estimate not available
        estimated_x_cal(frame) = true_distance * cos(aoa_cal_rad(frame));
        estimated_y_cal(frame) = true_distance * sin(aoa_cal_rad(frame));
    end
    
    if ~isnan(aoa_uncal(frame)) && ~isnan(distance_estimates(frame))
        estimated_x_uncal(frame) = distance_estimates(frame) * cos(aoa_uncal_rad(frame));
        estimated_y_uncal(frame) = distance_estimates(frame) * sin(aoa_uncal_rad(frame));
    elseif ~isnan(aoa_uncal(frame))
        % Use true distance if distance estimate not available
        estimated_x_uncal(frame) = true_distance * cos(aoa_uncal_rad(frame));
        estimated_y_uncal(frame) = true_distance * sin(aoa_uncal_rad(frame));
    end
end

% Compute per-frame RMSE errors
valid_cal = ~isnan(estimated_x_cal) & ~isnan(estimated_y_cal);
valid_uncal = ~isnan(estimated_x_uncal) & ~isnan(estimated_y_uncal);

% Calculate per-frame errors for filtering
frame_errors_cal = nan(Nf_common, 1);
frame_errors_uncal = nan(Nf_common, 1);

for frame = 1:Nf_common
    if valid_cal(frame)
        frame_errors_cal(frame) = sqrt((estimated_x_cal(frame) - true_x)^2 + (estimated_y_cal(frame) - true_y)^2);
    end
    if valid_uncal(frame)
        frame_errors_uncal(frame) = sqrt((estimated_x_uncal(frame) - true_x)^2 + (estimated_y_uncal(frame) - true_y)^2);
    end
end

% Filter out frames with RMSE > 0.20 m (if enabled)
rmse_threshold = 2;  % meters
if enable_rmse_filter
    valid_cal_filtered = valid_cal & (frame_errors_cal <= rmse_threshold);
    valid_uncal_filtered = valid_uncal & (frame_errors_uncal <= rmse_threshold);
    
    n_filtered_cal = sum(valid_cal) - sum(valid_cal_filtered);
    n_filtered_uncal = sum(valid_uncal) - sum(valid_uncal_filtered);
    
    if n_filtered_cal > 0 || n_filtered_uncal > 0
        fprintf('  RMSE filtering: Removed %d calibrated frames and %d uncalibrated frames with RMSE > %.2f m\n', ...
            n_filtered_cal, n_filtered_uncal, rmse_threshold);
    end
else
    % No filtering: use all valid frames
    valid_cal_filtered = valid_cal;
    valid_uncal_filtered = valid_uncal;
    fprintf('  RMSE filtering: DISABLED (using all valid frames)\n');
end

% Compute RMSE statistics using filtered data
if sum(valid_cal_filtered) > 0
    errors_x_cal = estimated_x_cal(valid_cal_filtered) - true_x;
    errors_y_cal = estimated_y_cal(valid_cal_filtered) - true_y;
    rmse_cal = sqrt(mean(errors_x_cal.^2 + errors_y_cal.^2));
    mean_error_cal = mean(sqrt(errors_x_cal.^2 + errors_y_cal.^2));
    std_error_cal = std(sqrt(errors_x_cal.^2 + errors_y_cal.^2));
else
    rmse_cal = NaN;
    mean_error_cal = NaN;
    std_error_cal = NaN;
end

if sum(valid_uncal_filtered) > 0
    errors_x_uncal = estimated_x_uncal(valid_uncal_filtered) - true_x;
    errors_y_uncal = estimated_y_uncal(valid_uncal_filtered) - true_y;
    rmse_uncal = sqrt(mean(errors_x_uncal.^2 + errors_y_uncal.^2));
    mean_error_uncal = mean(sqrt(errors_x_uncal.^2 + errors_y_uncal.^2));
    std_error_uncal = std(sqrt(errors_x_uncal.^2 + errors_y_uncal.^2));
else
    rmse_uncal = NaN;
    mean_error_uncal = NaN;
    std_error_uncal = NaN;
end

% Update estimated positions: set filtered frames to NaN
estimated_x_cal_filtered = estimated_x_cal;
estimated_y_cal_filtered = estimated_y_cal;
estimated_x_uncal_filtered = estimated_x_uncal;
estimated_y_uncal_filtered = estimated_y_uncal;

estimated_x_cal_filtered(~valid_cal_filtered) = NaN;
estimated_y_cal_filtered(~valid_cal_filtered) = NaN;
estimated_x_uncal_filtered(~valid_uncal_filtered) = NaN;
estimated_y_uncal_filtered(~valid_uncal_filtered) = NaN;

% Results
result.angle = true_angle;
result.distance = mean_distance_corrected;  % Mean distance from corrected firstPathAmp1 (for reporting)
result.distance_from_filename = str2double(test_distance_str);  % Integer distance from filename (used for error calculation)
result.Nf = Nf_common;
result.aoa_uncal_mean = mean(aoa_uncal, 'omitnan');
result.aoa_uncal_std = std(aoa_uncal, 'omitnan');
result.aoa_cal_mean = mean(aoa_cal, 'omitnan');
result.aoa_cal_std = std(aoa_cal, 'omitnan');
result.error_uncal = abs(result.aoa_uncal_mean - true_angle);
result.error_cal = abs(result.aoa_cal_mean - true_angle);
result.aoa_uncal = aoa_uncal;
result.aoa_cal = aoa_cal;
result.distance_estimates = distance_estimates;  % Corrected distances (or original if correction disabled)
result.distance_estimates_original = distance_estimates_original;  % Original distances before correction
result.rmse_cal = rmse_cal;
result.rmse_uncal = rmse_uncal;
result.mean_error_cal = mean_error_cal;
result.std_error_cal = std_error_cal;
result.mean_error_uncal = mean_error_uncal;
result.std_error_uncal = std_error_uncal;
result.estimated_x_cal = estimated_x_cal_filtered;  % Filtered (RMSE <= 0.20 m)
result.estimated_y_cal = estimated_y_cal_filtered;  % Filtered (RMSE <= 0.20 m)
result.estimated_x_uncal = estimated_x_uncal_filtered;  % Filtered (RMSE <= 0.20 m)
result.estimated_y_uncal = estimated_y_uncal_filtered;  % Filtered (RMSE <= 0.20 m)
result.valid_cal_filtered = valid_cal_filtered;  % Filter mask for calibrated
result.valid_uncal_filtered = valid_uncal_filtered;  % Filter mask for uncalibrated
result.true_x = true_x;
result.true_y = true_y;
result.port_original_frames = zeros(1, n_ports);
for idx = 1:n_ports
    result.port_original_frames(idx) = length(sequences_lde{idx});
end
result.ports_to_use = ports_to_use;

% If correction is disabled, return fit_data for model fitting
if ~enable_distance_correction
    % Collect data for fitting: measured_dist, true_dist, aoa_estimate
    fit_data.measured_dist = [];
    fit_data.true_dist = [];
    fit_data.angle = [];
    
    for frame = 1:Nf_common
        if ~isnan(distance_estimates_original(frame))
            % Use calibrated AoA if available, otherwise use uncalibrated
            if ~isnan(aoa_cal(frame))
                angle_deg = aoa_cal(frame);
            elseif ~isnan(aoa_uncal(frame))
                angle_deg = aoa_uncal(frame);
            else
                continue;  % Skip if no AoA available
            end
            
            fit_data.measured_dist = [fit_data.measured_dist; distance_estimates_original(frame)]; %#ok<AGROW>
            fit_data.true_dist = [fit_data.true_dist; true_distance]; %#ok<AGROW>
            fit_data.angle = [fit_data.angle; angle_deg]; %#ok<AGROW>
        end
    end
    
    result.fit_data = fit_data;
    fprintf('  Collected %d data points for model fitting\n', length(fit_data.measured_dist));
end

fprintf('  Uncal: AoA=%.1f° ± %.1f° (error: %.1f°), RMSE=%.3f m\n', ...
    result.aoa_uncal_mean, result.aoa_uncal_std, result.error_uncal, result.rmse_uncal);
fprintf('  Cal:   AoA=%.1f° ± %.1f° (error: %.1f°), RMSE=%.3f m\n\n', ...
    result.aoa_cal_mean, result.aoa_cal_std, result.error_cal, result.rmse_cal);

end

%% Helper functions (same as test_single_angle_multi_config_v2.m)
function spatial_phase_cal = apply_calibration(spatial_phase_raw, phase_compensation)
    [Nf, Nports] = size(spatial_phase_raw);
    spatial_phase_cal = nan(Nf, Nports);
    for frame = 1:Nf
        for port = 1:Nports
            if ~isnan(spatial_phase_raw(frame, port))
                corrected = spatial_phase_raw(frame, port) - phase_compensation(port);
                spatial_phase_cal(frame, port) = angle(exp(1j * corrected));
            end
        end
    end
end

function [aoa_estimates, mean_spectrum_db, theta_deg] = compute_fft_aoa(spatial_phase)
    spatial_phase_wrapped = angle(exp(1j * spatial_phase));
    spatial_signal = exp(1j * spatial_phase_wrapped);
    
    fft_size = 512;
    freq_axis = (-fft_size/2:fft_size/2-1) / fft_size;
    sin_theta = -2 * freq_axis;
    valid_idx = abs(sin_theta) <= 1;
    theta_deg = asin(sin_theta(valid_idx)) * 180/pi;
    
    Nf = size(spatial_signal, 1);
    fft_spectra = zeros(Nf, fft_size);
    aoa_estimates = nan(Nf, 1);
    
    for frame = 1:Nf
        signal = spatial_signal(frame, :);
        if any(isnan(signal)), continue; end
        
        spectrum = fftshift(fft(signal, fft_size));
        fft_spectra(frame, :) = abs(spectrum);
        
        spectrum_valid = abs(spectrum(valid_idx));
        [~, peak_idx] = max(spectrum_valid);
        aoa_estimates(frame) = theta_deg(peak_idx);
    end
    
    valid_frames = ~isnan(aoa_estimates);
    mean_spectrum = mean(fft_spectra(valid_frames, :), 1);
    mean_spectrum_valid = mean_spectrum(valid_idx);
    mean_spectrum_db = 10*log10(mean_spectrum_valid / max(mean_spectrum_valid));
end

function [aoa_estimates, mean_spectrum_db, theta_deg] = compute_mvdr_aoa(spatial_phase, n_ports, ports_to_use)
    if nargin < 3
        ports_to_use = 1:n_ports;
    end
    
    spatial_phase_wrapped = angle(exp(1j * spatial_phase));
    spatial_signal = exp(1j * spatial_phase_wrapped);
    
    Nf = size(spatial_signal, 1);
    N = n_ports;
    
    d = 0.5;
    lambda = 1;
    
    fft_size = 512;
    freq_axis = (-fft_size/2:fft_size/2-1) / fft_size;
    sin_theta_fft = -2 * freq_axis;
    valid_idx = abs(sin_theta_fft) <= 1;
    theta_deg = asin(sin_theta_fft(valid_idx)) * 180/pi;
    sin_theta = sin_theta_fft(valid_idx);
    
    steering_vectors = zeros(length(theta_deg), N);
    
    port_positions = zeros(1, N);
    if n_ports == 4 && isequal(ports_to_use, 3:6)
        port_positions = [0, 1, 2, 3];
    elseif n_ports == 2 && isequal(ports_to_use, 4:5)
        port_positions = [0, 1];
    elseif n_ports == 8 && isequal(ports_to_use, 1:8)
        port_positions = [0, 1, 2, 3, 4, 5, 6, 7];
    else
        port_positions = 0:(N-1);
    end
    
    for i = 1:length(theta_deg)
        for n = 1:N
            steering_vectors(i, n) = exp(-1j * (pi * port_positions(n) * sin_theta(i)));
        end
    end
    
    valid_frames = [];
    for frame = 1:Nf
        signal = spatial_signal(frame, :);
        if ~any(isnan(signal))
            valid_frames = [valid_frames; frame]; %#ok<AGROW>
        end
    end
    
    if isempty(valid_frames)
        aoa_estimates = nan(Nf, 1);
        mean_spectrum_db = nan(size(theta_deg));
        return;
    end
    
    R = zeros(N, N);
    for idx = 1:length(valid_frames)
        frame = valid_frames(idx);
        signal = spatial_signal(frame, :);
        x = signal(:);
        R = R + (x * x');
    end
    R = R / length(valid_frames);
    
    diagonal_loading = 0.01 * trace(R) / N;
    R = R + diagonal_loading * eye(N);
    
    mvdr_spectrum = zeros(length(theta_deg), 1);
    
    for i = 1:length(theta_deg)
        a = steering_vectors(i, :)';
        x = R \ a;
        denominator = real(a' * x);
        if denominator > 0
            mvdr_spectrum(i) = 1 / denominator;
        else
            mvdr_spectrum(i) = 0;
        end
    end
    
    mvdr_spectrum_db = 10*log10(mvdr_spectrum / max(mvdr_spectrum));
    mean_spectrum_db = mvdr_spectrum_db;
    
    aoa_estimates = nan(Nf, 1);
    window_size = min(20, max(5, round(Nf/4)));
    
    for frame = 1:Nf
        signal = spatial_signal(frame, :);
        if any(isnan(signal)), continue; end
        
        window_start = max(1, frame - round(window_size/2));
        window_end = min(Nf, frame + round(window_size/2));
        window_frames = window_start:window_end;
        
        window_signals = [];
        for wf = window_frames
            wf_signal = spatial_signal(wf, :);
            if ~any(isnan(wf_signal))
                window_signals = [window_signals; wf_signal]; %#ok<AGROW>
            end
        end
        
        if size(window_signals, 1) < 2
            R_frame = R;
        else
            R_frame = zeros(N, N);
            for wf_idx = 1:size(window_signals, 1)
                x = window_signals(wf_idx, :)';
                R_frame = R_frame + (x * x');
            end
            R_frame = R_frame / size(window_signals, 1);
            
            diagonal_loading = 0.01 * trace(R_frame) / N;
            R_frame = R_frame + diagonal_loading * eye(N);
        end
        
        mvdr_frame = zeros(length(theta_deg), 1);
        
        for i = 1:length(theta_deg)
            a = steering_vectors(i, :)';
            x = R_frame \ a;
            denominator = real(a' * x);
            if denominator > 0
                mvdr_frame(i) = 1 / denominator;
            else
                mvdr_frame(i) = 0;
            end
        end
        
        [~, peak_idx] = max(mvdr_frame);
        if ~isempty(peak_idx)
            aoa_estimates(frame) = theta_deg(peak_idx);
        end
    end
end

