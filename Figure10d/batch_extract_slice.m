function batch_extract_slice()
%BATCH_EXTRACT_SLICE  Figure10d rank1 RA slice (FFT).
%
% 20260218 square two-corner-reflectors, aoa=0 times=3.
% Row-aligned (no Sequence), clutter, streamed angle-FFT RA.
% 8/4/2 RX from the same 8-port cube (ports 1:8 / 1:4 / 1:2).
% Window 14, range ≈ 3.18 m. Writes angle_amplitude_{2,4,8}port.csv.

this_dir = fileparts(mfilename('fullpath'));
addpath(fullfile(this_dir, '..', '..', 'Range_doppler'));

data_root = fullfile(this_dir, 'raw');
calib_file = fullfile(data_root, 'calibration.csv');
if ~exist(calib_file, 'file')
    error('Missing calibration: %s', calib_file);
end
calib_full = load_complex_list(calib_file);

aoa_true = 0;
times = 3;
target_win = 14;
target_range = 3.1828125;

c = 3e8; fc = 3494.4e6; lambda = c / fc; fs = 64e9;
upsample_factor = 64; d = lambda / 2;
numCIRPoints = 100; frame_rate = 167; N_angle = 64;
lde_col = 10; cir_start = 11; origin_start_idx = 699;
win_radius = 83; RD_WINDOW = 83;
window_left = 10 * upsample_factor; window_right = 40 * upsample_factor;
min_range_zero = 0.6;

files = cell(8, 1);
for m = 1:8
    files{m} = fullfile(data_root, sprintf( ...
        'antenna_data_port%d_8ports_sensing_car_square_tworef_%d_%d.csv', ...
        m, aoa_true, times));
    if ~exist(files{m}, 'file')
        error('Missing CIR: %s', files{m});
    end
end

fprintf('Load 8-port aligned cube (aoa=%d times=%d)...\n', aoa_true, times);
[rx, ~, numFrames, ~] = load_and_align_window( ...
    files, numCIRPoints, upsample_factor, lde_col, cir_start, ...
    origin_start_idx, window_left, window_right, calib_full);
fprintf('Frames=%d\n', numFrames);
filtered = static_clutter_removal(rx, win_radius);
clear rx;

cfgs = {'8port', 1:8; '4port', 1:4; '2port', 1:2};
for i = 1:size(cfgs, 1)
    name = cfgs{i, 1};
    ports = cfgs{i, 2};
    pack = ra_one_window(filtered(ports, :, :), target_win, ...
        frame_rate, lambda, fs, RD_WINDOW, window_left, d, N_angle);
    amap = pack{1}; ra = pack{2}; theta_axis = pack{3};
    valid = ~isnan(theta_axis);
    theta = theta_axis(valid);
    amap = amap(valid, :);
    amap(:, ra < min_range_zero) = 0;
    [~, r_idx] = min(abs(ra - target_range));
    sl = amap(:, r_idx) / max(amap(:), [], 'omitnan');
    T = table(repmat(target_win, numel(theta), 1), ...
        repmat(ra(r_idx), numel(theta), 1), theta(:), sl(:), ...
        'VariableNames', {'Frame', 'Range_m', 'Angle_deg', 'Amplitude_normalized'});
    out = fullfile(this_dir, sprintf('angle_amplitude_%s.csv', name));
    writetable(T, out);
    [ymax, im] = max(sl);
    fprintf('  %s r=%.3f ymax=%.3f @ %.1f deg  -> %s\n', ...
        name, ra(r_idx), ymax, theta(im), out);
end
end


function pack = ra_one_window(filtered_all, win_idx, frame_rate, lambda, fs, RD_WINDOW, window_left, d, N_angle)
[M, window_len, numFrames] = size(filtered_all); %#ok<ASGLU>
start_list = 100 : 10 : (numFrames - RD_WINDOW + 1);
start_idx = start_list(win_idx);
idx_range = start_idx:(start_idx + RD_WINDOW - 1);
rd_cube = complex(zeros(M, window_len, RD_WINDOW));
for m = 1:M
    rd_buffer = squeeze(filtered_all(m, :, idx_range));
    rd_cube(m, :, :) = fftshift(fft(rd_buffer, [], 2), 2);
end
angle_full = fftshift(fft(rd_cube, N_angle, 1), 1);
angle_cube = abs(angle_full(2:end, :, :));
angle_cube(:, :, 41:43) = 0;
amap = max(angle_cube, [], 3);
k = -(N_angle/2-1):(N_angle/2-1);
sin_theta = k / N_angle * lambda / d;
theta_axis = asind(sin_theta);
theta_axis(abs(sin_theta) > 1) = NaN;
ra = (1:window_len) - (window_left + 1);
ra = ra / fs * 3e8 / 2;
pack = {amap, ra, theta_axis};
end


function calib = load_complex_list(path)
raw = readlines(path); raw = strtrim(raw); raw = raw(raw ~= "");
calib = zeros(numel(raw), 1);
for i = 1:numel(raw), calib(i) = complex(str2num(raw(i))); end %#ok<ST2NM>
end
