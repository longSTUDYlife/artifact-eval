function batch_extract_aoa(varargin)
%BATCH_EXTRACT_AOA  Figure10c sensing AoA (baseline) on 202603122 env1.
%
% Sequence-sync (seq_col=7 = firstPathAmp2) -> align+calib -> clutter
% -> streamed RA -> extract_aoa_from_angle_maps.
%
% Writes aoa_estimates_{M}port_{angle}.csv under aoa_estimates/.
%
%   batch_extract_aoa
%   batch_extract_aoa('angles', 0, 'ports', 8)

    p = inputParser;
    addParameter(p, 'angles', -40:10:40);
    addParameter(p, 'times', [1, 2, 3]);
    addParameter(p, 'ports', [8, 4, 2]);
    addParameter(p, 'min_range_m', 1.8);
    addParameter(p, 'ports_2', [3, 4]);
    addParameter(p, 'ports_4', [3, 4, 5, 6]);
    addParameter(p, 'ports_8', 1:8);
    addParameter(p, 'data_root', '');
    addParameter(p, 'calib_file', '');
    addParameter(p, 'file_pattern', 'antenna_data_port%d_8ports_sensing_env1_%d_%d.csv');
    addParameter(p, 'result_dir', '');
    addParameter(p, 'seq_col', 7);
    parse(p, varargin{:});

    aoa_list = p.Results.angles;
    times_list = p.Results.times;
    port_counts = p.Results.ports;
    min_range_m = p.Results.min_range_m;
    ports_2 = p.Results.ports_2(:)';
    ports_4 = p.Results.ports_4(:)';
    ports_8 = p.Results.ports_8(:)';
    file_pattern = char(p.Results.file_pattern);
    seq_col = p.Results.seq_col;

    this_dir = fileparts(mfilename('fullpath'));
    addpath(fullfile(this_dir, '..', '..', 'Range_doppler'));

    if isempty(p.Results.data_root)
        data_root = fullfile(this_dir, 'raw');
    else
        data_root = char(p.Results.data_root);
    end
    if isempty(p.Results.calib_file)
        calib_file = fullfile(data_root, 'spatial_phase_avg_complex_v3_angle0.csv');
        if ~exist(calib_file, 'file')
            calib_file = fullfile(this_dir, 'calibration.csv');
        end
    else
        calib_file = char(p.Results.calib_file);
    end
    if ~exist(calib_file, 'file')
        error('Missing calibration file: %s', calib_file);
    end
    calib_full = load_complex_list(calib_file);

    if isempty(p.Results.result_dir)
        result_dir = fullfile(this_dir, 'aoa_estimates');
    else
        result_dir = char(p.Results.result_dir);
    end
    if ~exist(result_dir, 'dir'), mkdir(result_dir); end

    fprintf('Data root: %s\n', data_root);
    fprintf('File pattern: %s\n', file_pattern);
    fprintf('Loaded calib (%d): %s\n', numel(calib_full), calib_file);
    fprintf('Sequence sync: ON (seq_col=%d)\n', seq_col);
    fprintf('Results -> %s\n', result_dir);

    c = 3e8;
    fc = 3494.4e6;
    lambda = c / fc;
    fs = 64e9;
    upsample_factor = 64;
    d = lambda / 2;
    numCIRPoints = 100;
    frame_rate = 167;
    N_angle = 64;
    lde_col = 10;
    cir_start = 11;
    origin_start_idx = 699;
    win_radius = 83;
    RD_WINDOW = 83;
    window_left = 10 * upsample_factor;
    window_right = 40 * upsample_factor;

    port_map = containers.Map({8, 4, 2}, {ports_8, ports_4, ports_2});

    tic;
    for pi = 1:numel(port_counts)
        M = port_counts(pi);
        if ~isKey(port_map, M)
            error('Unsupported port count %d', M);
        end
        ports = port_map(M);
        calib = calib_full(ports);
        fprintf('\n======== %d-port (ports %s) ========\n', M, mat2str(ports));

        for ai = 1:numel(aoa_list)
            aoa_true = aoa_list(ai);
            rows = [];

            for ti = 1:numel(times_list)
                times = times_list(ti);
                files = cell(M, 1);
                ok = true;
                for m = 1:M
                    files{m} = fullfile(data_root, sprintf( ...
                        file_pattern, ports(m), aoa_true, times));
                    if ~exist(files{m}, 'file')
                        ok = false;
                        break;
                    end
                end
                if ~ok
                    fprintf('  skip aoa=%d times=%d (missing files)\n', aoa_true, times);
                    continue;
                end

                try
                    [synced_data_all, common_seq] = sync_frames_by_sequence(files, seq_col);
                    fprintf('    synced %d common frames (seq [%d, %d])\n', ...
                        numel(common_seq), min(common_seq), max(common_seq));
                    [rx_signals_all, ~, numFrames, ~] = load_and_align_window_synced( ...
                        synced_data_all, numCIRPoints, upsample_factor, lde_col, cir_start, ...
                        origin_start_idx, window_left, window_right, calib);
                    clear synced_data_all common_seq;

                    if numFrames < RD_WINDOW
                        fprintf('  skip aoa=%d times=%d (frames=%d)\n', aoa_true, times, numFrames);
                        continue;
                    end

                    filtered_all = static_clutter_removal(rx_signals_all, win_radius);
                    clear rx_signals_all;
                    [angle_maps_all, range_axis_all, theta_axis] = compute_ra_maps_stream( ...
                        filtered_all, frame_rate, lambda, fs, RD_WINDOW, window_left, d, N_angle);
                    clear filtered_all;
                    numWin = numel(angle_maps_all);

                    if ~isempty(min_range_m) && isfinite(min_range_m)
                        for k = 1:numWin
                            if iscell(range_axis_all)
                                ra = range_axis_all{k}(:)';
                            else
                                ra = range_axis_all(:)';
                            end
                            angle_maps_all{k}(:, ra < min_range_m, :) = 0;
                        end
                    end

                    [aoa_est, range_est, energy_est] = extract_aoa_from_angle_maps( ...
                        angle_maps_all, range_axis_all, theta_axis, min_range_m);
                    clear angle_maps_all;

                    mae = mean(abs(aoa_est - aoa_true), 'omitnan');
                    fprintf('  aoa=%+3d times=%d: %d win  MAE=%.2f (%.1fs)\n', ...
                        aoa_true, times, numWin, mae, toc);

                    frame = (1:numWin)';
                    rows = [rows; frame, repmat(aoa_true, numWin, 1), ...
                        repmat(times, numWin, 1), aoa_est, range_est, energy_est]; %#ok<AGROW>
                catch ME
                    fprintf('  ERROR aoa=%d times=%d: %s\n', aoa_true, times, ME.message);
                end
            end

            if isempty(rows)
                continue;
            end
            names = {'frame', 'aoa', 'times', 'estimated_aoa', 'range', 'energy'};
            T = array2table(rows, 'VariableNames', names);
            stem = sprintf('aoa_estimates_%dport_%d', M, aoa_true);
            writetable(T, fullfile(result_dir, [stem '.csv']));
            fprintf('  -> %s.csv  N=%d  MAE=%.2f\n', ...
                stem, height(T), mean(abs(T.estimated_aoa - T.aoa), 'omitnan'));
        end
    end
    fprintf('\nDone in %.1fs\n', toc);
end


function calib = load_complex_list(path)
raw = readlines(path);
raw = strtrim(raw);
raw = raw(raw ~= "");
calib = zeros(numel(raw), 1);
for i = 1:numel(raw)
    calib(i) = complex(str2num(raw(i))); %#ok<ST2NM>
end
end
