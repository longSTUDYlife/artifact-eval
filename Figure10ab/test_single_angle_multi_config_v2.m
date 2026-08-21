function [result] = test_single_angle_multi_config_v2(test_angle_str, phase_compensation, n_ports, ports_to_use, method)
% Test a single angle using pre-computed phase compensation
% WITH SEQUENCE-BASED ALIGNMENT for handling dropped frames
% Uses the SAME alignment method as process_unified_data_and_calibrate.m (forward_dist method)
% Supports different number of ports: 8, 4, or 2
% n_ports: number of ports to use (8, 4, or 2)
% ports_to_use: array of port numbers to use (e.g., [1:8], [3:6], [4:5])
% method: 'fft' (default) or 'mvdr' for AoA estimation method
% Returns: result struct with mean, std, error

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

if ~ismember(method, {'fft', 'mvdr', 'mvdr_enhanced'})
    error('method must be ''fft'', ''mvdr'', or ''mvdr_enhanced''');
end

method_display = upper(strrep(method, '_', ' '));
fprintf('Testing angle: %s° (%d ports: [%s], method: %s, with sequence alignment v2, proper LDE)...\n', ...
    test_angle_str, n_ports, num2str(ports_to_use), method_display);

% Local copy of MultiPort concurrent localization CSVs (Figure10ab/raw)
script_dir = fileparts(mfilename('fullpath'));
data_path = fullfile(script_dir, 'raw');
if data_path(end) ~= filesep
    data_path = [data_path filesep];
end

% LDE intermediates live under lde_cache/
lde_dir = fullfile(script_dir, 'lde_cache');
if ~exist(lde_dir, 'dir')
    mkdir(lde_dir);
end

%% Step 1: Extract LDE using proper algorithm (export_lde_complex_8antenna)
fprintf('  Extracting LDE using proper algorithm...\n');
fprintf('  Raw data path: %s\n', data_path);
fprintf('  LDE files will be saved to: %s\n', lde_dir);

% Ensure export_lde_complex_8antenna function is in path
if ~exist('export_lde_complex_8antenna', 'file')
    addpath(script_dir);
    if ~exist('export_lde_complex_8antenna', 'file')
        error('export_lde_complex_8antenna function not found in path!');
    end
end

for port = ports_to_use
    csvfile = fullfile(lde_dir, sprintf('lde_complex_real_port%d_angle%s.csv', port, test_angle_str));
    if ~exist(csvfile, 'file')
        fprintf('    Port %d: Extracting LDE...\n', port);
        infile = fullfile(data_path, sprintf('antenna_data_port%d_8ports_concurrent_localization_aoa_accuracy_%s.csv', port, test_angle_str));
        if ~exist(infile, 'file')
            fprintf('  ERROR: Data file not found for port %d!\n', port);
            fprintf('    Expected file: %s\n', infile);
            result = [];
            return;
        end
        try
            export_lde_complex_8antenna(infile, csvfile);
            % Verify file was created
            if ~exist(csvfile, 'file')
                error('LDE extraction completed but output file was not created: %s', csvfile);
            end
            fprintf('    Port %d: LDE extraction completed successfully\n', port);
        catch ME
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

%% Step 2: Load LDE data AND original sequence numbers
fprintf('  Loading LDE data and sequence numbers...\n');

sequences_lde = cell(n_ports, 1);
lde_data = cell(n_ports, 1);

for idx = 1:n_ports
    port = ports_to_use(idx);
    % Load LDE results
    csvfile = fullfile(lde_dir, sprintf('lde_complex_real_port%d_angle%s.csv', port, test_angle_str));
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
    
    % Load original data to get sequence numbers
    infile = fullfile(data_path, sprintf('antenna_data_port%d_8ports_concurrent_localization_aoa_accuracy_%s.csv', port, test_angle_str));
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
    
    if ~ismember('Sequence', T_orig.Properties.VariableNames)
        fprintf('  ERROR: Sequence column not found in port %d data!\n', port);
        result = [];
        return;
    end
    
    % Store sequences (should match LDE row count)
    sequences_lde{idx} = T_orig.Sequence;
    
    if height(T_lde) ~= height(T_orig)
        fprintf('  ERROR: LDE and original data have different row counts for port %d!\n', port);
        result = [];
        return;
    end
end

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
is_empty_frame = cell(n_ports, 1);

for idx = 1:n_ports
    seqs_orig = sequences_lde{idx};
    lde_orig = lde_data{idx};
    
    padded_table = cell(Nf_padded, 1);
    padded_seqs = zeros(Nf_padded, 1);
    is_empty = false(Nf_padded, 1);
    
    orig_pos = 1;  % Pointer to current position in original data
    
    for i = 1:Nf_padded
        seq = seq_range(i);
        padded_seqs(i) = seq;
        
        % Check if current position matches
        if orig_pos <= length(seqs_orig) && seqs_orig(orig_pos) == seq
            % Match found, use original data
            padded_table{i} = lde_orig(orig_pos, :);
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
end

%% Step 4.5: LDE Quality Filtering
fprintf('  Applying LDE quality filtering...\n');
% Filter out frames that don't have two LDEs detected or LDE distance <= 30
% (same as process_unified_data_and_calibrate.m)

% LDE distance threshold (in bins)
min_lde_distance = 30;

% Load original CIR data for distance calculation
% We need to map aligned frames back to original frames
fprintf('    Loading original CIR data for LDE distance calculation...\n');
original_cir_data = cell(n_ports, 1);
for idx = 1:n_ports
    port = ports_to_use(idx);
    infile = fullfile(data_path, sprintf('antenna_data_port%d_8ports_concurrent_localization_aoa_accuracy_%s.csv', port, test_angle_str));
    if ~exist(infile, 'file')
        fprintf('  WARNING: Cannot load original data for port %d, skipping distance check\n', port);
        original_cir_data{idx} = [];
        continue;
    end
    try
        T_orig = readtable(infile);
        % Extract CIR columns
        names = T_orig.Properties.VariableNames;
        isR = startsWith(names, 'CIR_real_');
        isI = startsWith(names, 'CIR_imag_');
        rNames = names(isR); iNames = names(isI);
        rIdx = sscanf(strjoin(erase(rNames, 'CIR_real_'), ' '), '%d'); [~, ordR] = sort(rIdx); rNames = rNames(ordR);
        iIdx = sscanf(strjoin(erase(iNames, 'CIR_imag_'), ' '), '%d'); [~, ordI] = sort(iIdx); iNames = iNames(ordI);
        R = double(T_orig{:, rNames});
        I = double(T_orig{:, iNames});
        original_cir_data{idx} = complex(R, I);
    catch ME
        fprintf('  WARNING: Failed to load original data for port %d: %s\n', port, ME.message);
        original_cir_data{idx} = [];
    end
end

% LDE detection parameters (same as export_lde_complex_8antenna)
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

L = 64;
interpMethod = 'pchip';
noiseRegionBins = 10;
mergeGapBelow = 2;
minStayAbove = 3;
earlyPeakFrac = 0.80;
gradSpan = max(1, P.gradWin);

% Helper function for magnitude approximation
approx_mag = @(xc) max(abs(real(xc)), abs(imag(xc))) + 0.25*min(abs(real(xc)), abs(imag(xc)));

% Check each frame: all ports must have two LDEs and distance > 30
valid_frame_mask = true(Nf_common, 1);
n_single_lde = 0;
n_distance_invalid = 0;

for frame = 1:Nf_common
    % Map aligned frame back to original frame index
    % aligned_final_indices(frame) is the index in padded data
    % We need to find the corresponding original frame
    % This is complex because of padding, so we'll check each port separately
    
    all_valid = true;
    for idx = 1:n_ports
        T_aligned = padded_lde_data{idx}(aligned_final_indices(frame), :);
        
        % Check if two LDEs exist
        has_two_lde = ~isnan(T_aligned.complex_large_real) && ~isnan(T_aligned.complex_large_imag);
        if ~has_two_lde
            all_valid = false;
            n_single_lde = n_single_lde + 1;
            break;
        end
        
        % Check LDE distance if we have original CIR data
        if ~isempty(original_cir_data{idx})
            % Find the original frame index
            % aligned_final_indices(frame) corresponds to a frame in padded_sequences{idx}
            padded_seq = padded_sequences{idx}(aligned_final_indices(frame));
            % Find this sequence in original sequences
            orig_frame_idx = find(sequences_lde{idx} == padded_seq, 1);
            
            if ~isempty(orig_frame_idx) && orig_frame_idx <= size(original_cir_data{idx}, 1)
                try
                    % Recalculate LDE positions
                    x = original_cir_data{idx}(orig_frame_idx, :);
                    mag = approx_mag(x);
                    Nbins = numel(mag);
                    
                    % LDE detection (simplified version)
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
                        z = ~above; dz = diff([0 z 0]);
                        zs = find(dz == 1); ze = find(dz == -1) - 1;
                        for ii = 1:numel(zs)
                            Lg = zs(ii); Rg = ze(ii); gapLen = Rg - Lg + 1;
                            leftOK = (Lg - 1) >= 1 && above(Lg - 1);
                            rightOK = (Rg + 1) <= Nbins && above(Rg + 1);
                            if gapLen <= mergeGapBelow && leftOK && rightOK
                                above(Lg:Rg) = true;
                            end
                        end
                    end
                    
                    d = diff([0 above 0]);
                    st = find(d == 1); en = find(d == -1) - 1;
                    
                    if ~isempty(st) && numel(st) >= 2
                        % Get top 2 LDEs
                        candIdx = [];
                        candAmp = [];
                        for c = 1:numel(st)
                            s_bin = st(c); e_bin = en(c);
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
                            
                            g = diff(mag);
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
                        
                        if length(candIdx) >= 2
                            [~, srt] = sort(candAmp, 'descend');
                            srt = srt(1:min(2, numel(srt)));
                            ldes = candIdx(srt);
                            [ldes_sorted, ~] = sort(ldes, 'ascend');
                            delta = ldes_sorted(2) - ldes_sorted(1);
                            
                            if delta <= min_lde_distance
                                all_valid = false;
                                n_distance_invalid = n_distance_invalid + 1;
                                break;
                            end
                        end
                    end
                catch
                    % If LDE recalculation fails, skip distance check for this frame
                    % (assume valid if two LDEs exist)
                end
            end
        end
    end
    
    if ~all_valid
        valid_frame_mask(frame) = false;
    end
end

n_filtered = sum(~valid_frame_mask);
n_remaining = sum(valid_frame_mask);
fprintf('    Filtered out %d frames:', n_filtered);
if n_single_lde > 0
    fprintf(' %d frames with only 1 LDE', n_single_lde);
end
if n_distance_invalid > 0
    fprintf(' %d frames with LDE distance <= %d', n_distance_invalid, min_lde_distance);
end
fprintf('\n');
fprintf('    Remaining %d valid frames\n', n_remaining);

if n_remaining == 0
    fprintf('  ERROR: No valid frames after LDE filtering!\n');
    result = [];
    return;
end

% Apply filtering: keep only valid frames
aligned_final_indices = aligned_final_indices(valid_frame_mask);
Nf_common = n_remaining;

% Re-extract aligned LDE data with filtered indices
for idx = 1:n_ports
    T_aligned = padded_lde_data{idx}(aligned_final_indices, :);
    port_data{idx}.phase_small = angle(complex(T_aligned.complex_small_real, T_aligned.complex_small_imag));
    port_data{idx}.phase_large = angle(complex(T_aligned.complex_large_real, T_aligned.complex_large_imag));
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
    % phase_compensation is already for the ports we're using (e.g., for 4-port config,
    % it's already [comp3, comp4, comp5, comp6], not the full 8-port array)
    spatial_phase_cal = apply_calibration(spatial_phase_raw, phase_compensation);
else
    % No calibration available or mismatch, use raw
    fprintf('  Warning: Phase compensation length (%d) does not match n_ports (%d), using raw data\n', ...
        length(phase_compensation), n_ports);
    spatial_phase_cal = spatial_phase_raw;
end

%% Step 7: Compute AoA
true_angle = str2double(test_angle_str);

if strcmpi(method, 'mvdr')
    % Standard MVDR (Minimum Variance Distortionless Response)
    [aoa_uncal, ~, ~] = compute_mvdr_aoa(spatial_phase_raw, n_ports, ports_to_use);
    [aoa_cal, ~, ~] = compute_mvdr_aoa(spatial_phase_cal, n_ports, ports_to_use);
elseif strcmpi(method, 'mvdr_enhanced')
    % Enhanced MVDR with multiple improvements for low-port configurations
    % 
    % NOTE: If enhanced MVDR performs worse, try these adjustments:
    % 1. Disable spatial smoothing (set use_spatial_smoothing = false)
    % 2. Reduce window_size_multiplier (closer to 1.0)
    % 3. Reduce diagonal_loading_factor (closer to 0.01)
    % 4. Disable forward-backward averaging if needed
    
    % Configure enhanced MVDR options - MINIMAL ENHANCEMENT VERSION
    % This version only adds angle search refinement and slightly larger windows
    % All other enhancements are disabled by default
    mvdr_options = struct();
    
    % Minimal enhancement: only angle refinement and slightly larger windows
    mvdr_options.use_forward_backward = false;  % Disabled - can cause issues
    mvdr_options.use_spatial_smoothing = false;  % Disabled - can degrade performance
    mvdr_options.angle_search_refinement = true;  % Keep - generally safe
    
    % Very conservative window size: almost same as standard MVDR
    if n_ports == 2
        mvdr_options.window_size_multiplier = 1.2;  % Very small increase
        mvdr_options.diagonal_loading_factor = 0.015;  % Slightly higher for stability
    elseif n_ports == 4
        mvdr_options.window_size_multiplier = 1.1;  % Minimal increase
        mvdr_options.diagonal_loading_factor = 0.012;  % Slightly higher
    else
        mvdr_options.window_size_multiplier = 1.0;  % Same as standard
        mvdr_options.diagonal_loading_factor = 0.01;  % Same as standard
    end
    
    mvdr_options.use_music = false;  % Use MVDR
    
    % Check if enhanced function exists
    if exist('compute_mvdr_aoa_enhanced', 'file')
        [aoa_uncal, ~, ~] = compute_mvdr_aoa_enhanced(spatial_phase_raw, n_ports, ports_to_use, mvdr_options);
        [aoa_cal, ~, ~] = compute_mvdr_aoa_enhanced(spatial_phase_cal, n_ports, ports_to_use, mvdr_options);
    else
        % Fallback to standard MVDR
        fprintf('  Warning: Enhanced MVDR function not found, using standard MVDR\n');
        fprintf('  Please ensure compute_mvdr_aoa_enhanced.m is in the path\n');
        [aoa_uncal, ~, ~] = compute_mvdr_aoa(spatial_phase_raw, n_ports, ports_to_use);
        [aoa_cal, ~, ~] = compute_mvdr_aoa(spatial_phase_cal, n_ports, ports_to_use);
    end
else
    % Default: FFT method
    [aoa_uncal, ~, ~] = compute_fft_aoa(spatial_phase_raw);
    [aoa_cal, ~, ~] = compute_fft_aoa(spatial_phase_cal);
end

% Results
result.angle = true_angle;
result.Nf = Nf_common;
result.aoa_uncal_mean = mean(aoa_uncal, 'omitnan');
result.aoa_uncal_std = std(aoa_uncal, 'omitnan');
result.aoa_cal_mean = mean(aoa_cal, 'omitnan');
result.aoa_cal_std = std(aoa_cal, 'omitnan');
result.error_uncal = abs(result.aoa_uncal_mean - true_angle);
result.error_cal = abs(result.aoa_cal_mean - true_angle);
% Store per-frame AoA estimates for CDF analysis
result.aoa_uncal = aoa_uncal;
result.aoa_cal = aoa_cal;
% Store original frame counts per port for debugging
result.port_original_frames = zeros(1, n_ports);
for idx = 1:n_ports
    result.port_original_frames(idx) = length(sequences_lde{idx});
end
result.ports_to_use = ports_to_use;

fprintf('  Uncal: %.1f° ± %.1f° (error: %.1f°)\n', ...
    result.aoa_uncal_mean, result.aoa_uncal_std, result.error_uncal);
fprintf('  Cal:   %.1f° ± %.1f° (error: %.1f°)\n\n', ...
    result.aoa_cal_mean, result.aoa_cal_std, result.error_cal);

end

%% Helper functions
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
    sin_theta = -2 * freq_axis;  % Fixed: negative sign to correct angle direction
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
    % MVDR (Minimum Variance Distortionless Response) AoA estimation
    % Also known as Capon beamformer
    % ports_to_use: array of physical port numbers (e.g., [1:8], [3:6], [4:5])
    
    if nargin < 3
        ports_to_use = 1:n_ports;  % Default: assume ports 1 to n_ports
    end
    
    % Convert phase to complex signal (same as FFT)
    spatial_phase_wrapped = angle(exp(1j * spatial_phase));
    spatial_signal = exp(1j * spatial_phase_wrapped);
    
    Nf = size(spatial_signal, 1);
    N = n_ports;  % Number of antennas
    
    % Antenna spacing: half wavelength (d = lambda/2)
    d = 0.5;  % normalized by wavelength
    lambda = 1;  % normalized wavelength
    
    % Generate angle grid matching FFT's angle mapping
    % FFT uses: sin_theta = -2 * freq_axis, so we need to match this relationship
    % For consistency with FFT, use the same angle grid
    fft_size = 512;
    freq_axis = (-fft_size/2:fft_size/2-1) / fft_size;
    sin_theta_fft = -2 * freq_axis;  % Match FFT's sign convention
    valid_idx = abs(sin_theta_fft) <= 1;
    theta_deg = asin(sin_theta_fft(valid_idx)) * 180/pi;
    sin_theta = sin_theta_fft(valid_idx);  % Use the same sin_theta values as FFT
    
    % Steering vectors for each angle
    % For linear array with half-wavelength spacing:
    % a(theta) = [1, exp(j*2*pi*d*sin(theta)/lambda), exp(j*2*pi*2*d*sin(theta)/lambda), ...]
    % Note: spatial_phase is already relative to first port (port 1 = reference, phase = 0)
    % So steering vector first element is always 1
    % 
    % Important: FFT uses sin_theta = -2 * freq_axis, and theta_deg = asin(sin_theta)
    % This means sin_theta already has the correct sign for the angle mapping.
    % For half-wavelength spacing (d = lambda/2), the phase difference is:
    % phase_diff = 2*pi*n*d*sin(theta)/lambda = pi*n*sin(theta)
    % Since sin_theta already comes from asin() with the correct sign, we use it directly
    %
    % For 4-port config with ports [3, 4, 5, 6], the physical positions might be different
    % We need to account for the actual port positions relative to port 3 (reference)
    steering_vectors = zeros(length(theta_deg), N);
    
    % Calculate relative positions of ports
    % For 8-port array, ports are at positions: 0, 1, 2, 3, 4, 5, 6, 7 (in half-wavelength units)
    % For 4-port config [3, 4, 5, 6], relative to port 3:
    %   Port 3 (idx=1): position 0 (reference)
    %   Port 4 (idx=2): position 1 (relative to port 3)
    %   Port 5 (idx=3): position 2 (relative to port 3)
    %   Port 6 (idx=4): position 3 (relative to port 3)
    port_positions = zeros(1, N);
    if n_ports == 4 && isequal(ports_to_use, 3:6)
        % 4-port config: ports 3, 4, 5, 6
        % Relative positions: 0, 1, 2, 3 (in half-wavelength units from port 3)
        port_positions = [0, 1, 2, 3];
    elseif n_ports == 2 && isequal(ports_to_use, 4:5)
        % 2-port config: ports 4, 5
        % Relative positions: 0, 1 (in half-wavelength units from port 4)
        port_positions = [0, 1];
    elseif n_ports == 8 && isequal(ports_to_use, 1:8)
        % 8-port config: ports 1, 2, 3, 4, 5, 6, 7, 8
        % Relative positions: 0, 1, 2, 3, 4, 5, 6, 7 (in half-wavelength units from port 1)
        port_positions = [0, 1, 2, 3, 4, 5, 6, 7];
    else
        % Default: assume uniform spacing starting from 0
        port_positions = 0:(N-1);
    end
    
    for i = 1:length(theta_deg)
        for n = 1:N
            % Use actual port position relative to reference port
            % phase_diff = 2*pi*position*d*sin(theta)/lambda = pi*position*sin(theta)
            steering_vectors(i, n) = exp(-1j * (pi * port_positions(n) * sin_theta(i)));
        end
    end
    
    % Estimate covariance matrix from all frames
    % Use sample covariance matrix
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
    
    % Compute sample covariance matrix
    % R = E[x * x^H] where x is the spatial signal vector (row vector)
    % For row vector x, we need x' * x (not x * x')
    R = zeros(N, N);
    for idx = 1:length(valid_frames)
        frame = valid_frames(idx);
        signal = spatial_signal(frame, :);  % Row vector: 1 x N
        % For row vector: x' * x gives x^H * x (N x N)
        % But we need x * x^H, so we use: signal(:) * signal(:)'
        % signal(:) converts row to column, signal(:)' is row
        x = signal(:);  % Column vector: N x 1
        R = R + (x * x');  % x * x^H: N x N
    end
    R = R / length(valid_frames);
    
    % Add diagonal loading for numerical stability
    % Use a small fraction of the trace to regularize
    diagonal_loading = 0.01 * trace(R) / N;  % Increased from 1e-6 for better stability
    R = R + diagonal_loading * eye(N);
    
    % Compute MVDR spectrum
    % Use more stable computation: solve R * x = a, then compute a^H * x
    mvdr_spectrum = zeros(length(theta_deg), 1);
    
    for i = 1:length(theta_deg)
        a = steering_vectors(i, :)';  % Steering vector for this angle
        % MVDR power: P = 1 / (a^H * R^(-1) * a)
        % More stable: solve R * x = a, then P = 1 / (a^H * x)
        x = R \ a;
        denominator = real(a' * x);
        if denominator > 0
            mvdr_spectrum(i) = 1 / denominator;
        else
            mvdr_spectrum(i) = 0;
        end
    end
    
    % Normalize spectrum
    mvdr_spectrum_db = 10*log10(mvdr_spectrum / max(mvdr_spectrum));
    mean_spectrum_db = mvdr_spectrum_db;
    
    % Per-frame AoA estimation
    % For MVDR, we need to compute per-frame covariance matrices to get per-frame AoA estimates
    % Standard MVDR uses: P(theta) = 1 / (a^H(theta) * R^(-1) * a(theta))
    % If we use the same R for all frames, all frames will have the same spectrum and AoA estimate
    % To get per-frame estimates, we need per-frame covariance matrices
    aoa_estimates = nan(Nf, 1);
    
    % Use a sliding window to compute per-frame covariance matrices
    % Window size: use a reasonable number of frames (e.g., min(20, Nf/4))
    window_size = min(20, max(5, round(Nf/4)));
    
    for frame = 1:Nf
        signal = spatial_signal(frame, :);
        if any(isnan(signal)), continue; end
        
        % Compute per-frame covariance matrix using a sliding window
        % Window centered at current frame
        window_start = max(1, frame - round(window_size/2));
        window_end = min(Nf, frame + round(window_size/2));
        window_frames = window_start:window_end;
        
        % Collect signals in the window
        window_signals = [];
        for wf = window_frames
            wf_signal = spatial_signal(wf, :);
            if ~any(isnan(wf_signal))
                window_signals = [window_signals; wf_signal]; %#ok<AGROW>
            end
        end
        
        if size(window_signals, 1) < 2
            % Not enough frames in window, use global covariance
            R_frame = R;
        else
            % Compute covariance matrix for this window
            R_frame = zeros(N, N);
            for wf_idx = 1:size(window_signals, 1)
                x = window_signals(wf_idx, :)';  % Column vector: N x 1
                R_frame = R_frame + (x * x');  % x * x^H: N x N
            end
            R_frame = R_frame / size(window_signals, 1);
            
            % Add diagonal loading for numerical stability
            diagonal_loading = 0.01 * trace(R_frame) / N;
            R_frame = R_frame + diagonal_loading * eye(N);
        end
        
        % Compute MVDR spectrum for this frame
        % Use more stable computation: solve R_frame * x = a, then compute a^H * x
        mvdr_frame = zeros(length(theta_deg), 1);
        
        for i = 1:length(theta_deg)
            a = steering_vectors(i, :)';  % Column vector: N x 1
            % More stable: solve R_frame * x = a, then P = 1 / (a^H * x)
            x = R_frame \ a;
            denominator = real(a' * x);  % a^H * x (scalar)
            if denominator > 0
                mvdr_frame(i) = 1 / denominator;
            else
                mvdr_frame(i) = 0;
            end
        end
        
        % Find peak
        [~, peak_idx] = max(mvdr_frame);
        if ~isempty(peak_idx)
            aoa_estimates(frame) = theta_deg(peak_idx);
        end
    end
end

