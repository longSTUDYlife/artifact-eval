function compute_cable_phase_fluctuation()
% Compute two-port cable phase fluctuation using LDE-referenced tap phase.
%
% Per frame and per port:
%   relative_phase = phase(CIR at LDE-relative multipath) - phase(CIR at LDE index)
%
% Final two-port quantity:
%   double_difference = relative_phase_port1 - relative_phase_port2

script_dir = fileparts(mfilename('fullpath'));
port1_csv = fullfile(script_dir, 'cable_data_port1_fp_mp.csv');
port2_csv = fullfile(script_dir, 'cable_data_port2_fp_mp.csv');

first_cir_tap = 699;
port1_relative_offset = 1584 / 64;
port2_relative_offset = 1778 / 64;
aligned_frame_range = [7000, 9000];  % Select aligned frame indices, e.g. [1, 300].

if ~exist(port1_csv, 'file')
    error('Cannot find port1 CSV: %s', port1_csv);
end
if ~exist(port2_csv, 'file')
    error('Cannot find port2 CSV: %s', port2_csv);
end

fprintf('Loading port 1: %s\n', port1_csv);
P1 = load_port_phase_data(port1_csv, port1_relative_offset, first_cir_tap);
fprintf('Loading port 2: %s\n', port2_csv);
P2 = load_port_phase_data(port2_csv, port2_relative_offset, first_cir_tap);

[idx1, idx2] = align_two_by_sequence(P1.sequence, P2.sequence);
if isempty(idx1)
    error('No aligned frames found between port 1 and port 2.');
end
n_aligned_available = numel(idx1);
frame_start = max(1, aligned_frame_range(1));
frame_end = min(aligned_frame_range(2), n_aligned_available);
if frame_start > frame_end
    error('Selected aligned frame range [%d, %d] is outside available aligned frames 1-%d.', ...
        aligned_frame_range(1), aligned_frame_range(2), n_aligned_available);
end
aligned_frame_index = (frame_start:frame_end).';
idx1 = idx1(aligned_frame_index);
idx2 = idx2(aligned_frame_index);

sequence = P1.sequence(idx1);
phase_lde1 = P1.phase_lde(idx1);
phase_lde2 = P2.phase_lde(idx2);
phase_target1 = P1.phase_target(idx1);
phase_target2 = P2.phase_target(idx2);
lde_abs_idx1 = P1.lde_abs_idx(idx1);
lde_abs_idx2 = P2.lde_abs_idx(idx2);
lde_local_tap1 = P1.lde_local_tap(idx1);
lde_local_tap2 = P2.lde_local_tap(idx2);
target_abs_idx1 = P1.target_abs_idx(idx1);
target_abs_idx2 = P2.target_abs_idx(idx2);
target_local_tap1 = P1.target_local_tap(idx1);
target_local_tap2 = P2.target_local_tap(idx2);

relative_phase_raw = nan(numel(sequence), 2);
relative_phase_raw(:, 1) = phase_target1 - phase_lde1;
relative_phase_raw(:, 2) = phase_target2 - phase_lde2;

relative_phase1 = wrap_phase(relative_phase_raw(:, 1));
relative_phase2 = wrap_phase(relative_phase_raw(:, 2));
double_difference = wrap_phase(relative_phase_raw(:, 1) - relative_phase_raw(:, 2));

valid = ~isnan(double_difference);

fprintf('\nfirstCIRTap: %d\n', first_cir_tap);
fprintf('Port 1 relative target offset: %.6f taps\n', port1_relative_offset);
fprintf('Port 2 relative target offset: %.6f taps\n', port2_relative_offset);
fprintf('Available aligned frames: %d\n', n_aligned_available);
fprintf('Selected aligned frame range: %d-%d\n', frame_start, frame_end);
fprintf('Selected frames: %d\n', numel(sequence));
fprintf('Valid frames: %d\n', sum(valid));
print_phase_stats(sprintf('Port 1: phase(LDE+%.3f) - phase(LDE)', port1_relative_offset), relative_phase1(valid));
print_phase_stats(sprintf('Port 2: phase(LDE+%.3f) - phase(LDE)', port2_relative_offset), relative_phase2(valid));
print_phase_stats('Double difference', double_difference(valid));

out_csv = fullfile(script_dir, 'cable_phase_fluctuation_relative_tap.csv');
outT = table(aligned_frame_index, sequence, idx1(:), idx2(:), lde_abs_idx1, lde_abs_idx2, lde_local_tap1, lde_local_tap2, ...
    target_abs_idx1, target_abs_idx2, target_local_tap1, target_local_tap2, ...
    phase_lde1, phase_lde2, phase_target1, phase_target2, ...
    relative_phase1, relative_phase2, double_difference, valid, ...
    'VariableNames', {'Aligned_Frame_Index','Sequence','Port1_Row','Port2_Row','LDE_Abs_Index_Port1','LDE_Abs_Index_Port2', ...
    'LDE_Local_Tap_Port1','LDE_Local_Tap_Port2', ...
    'Target_Abs_Index_Port1','Target_Abs_Index_Port2','Target_Local_Tap_Port1','Target_Local_Tap_Port2', ...
    'Phase_LDE_Port1','Phase_LDE_Port2','Phase_Target_Port1','Phase_Target_Port2', ...
    'Relative_Phase_Port1','Relative_Phase_Port2','Double_Difference','Valid'});
writetable(outT, out_csv);
fprintf('\nWrote results: %s\n', out_csv);

plot_phase_results(aligned_frame_index(valid), relative_phase1(valid), relative_phase2(valid), ...
    double_difference(valid), script_dir, port1_relative_offset, port2_relative_offset);

fprintf('Done.\n');
end

function P = load_port_phase_data(csv_file, relative_offset, first_cir_tap)
T = readtable(csv_file);
names = T.Properties.VariableNames;

real_cols = names(startsWith(names, 'CIR_real_'));
imag_cols = names(startsWith(names, 'CIR_imag_'));
real_idx = sscanf(strjoin(erase(real_cols, 'CIR_real_'), ' '), '%d');
imag_idx = sscanf(strjoin(erase(imag_cols, 'CIR_imag_'), ' '), '%d');
[real_idx_sorted, ordR] = sort(real_idx);
[imag_idx_sorted, ordI] = sort(imag_idx);
real_cols = real_cols(ordR);
imag_cols = imag_cols(ordI);

if ~isequal(real_idx_sorted, imag_idx_sorted)
    error('CIR real/imag columns do not match in %s', csv_file);
end
R = double(T{:, real_cols});
I = double(T{:, imag_cols});
CIR = complex(R, I);
nbins = size(CIR, 2);

if ~ismember('firstPath', names)
    error('firstPath column not found in %s', csv_file);
end
if ~ismember('rxPreamCount', names)
    error('rxPreamCount column not found in %s', csv_file);
end
first_path = double(T.firstPath);
rx_pream_count = double(T.rxPreamCount);
lde_abs_idx = first_path;
lde_local_tap = lde_abs_idx - first_cir_tap + 1;
target_abs_idx = lde_abs_idx + relative_offset;
target_local_tap = lde_local_tap + relative_offset;

phase_target = nan(height(T), 1);
phase_lde = nan(height(T), 1);
for k = 1:height(T)
    if isnan(lde_local_tap(k)) || lde_local_tap(k) < 1 || lde_local_tap(k) > nbins || ...
            target_local_tap(k) < 1 || target_local_tap(k) > nbins
        continue;
    end

    x_up = resample(CIR(k, :), 64, 1) / rx_pream_count(k);
    lde_up_idx = round(lde_local_tap(k) * 64);
    target_up_idx = lde_up_idx + round(relative_offset * 64);

    lde_complex = sample_resampled_by_index(x_up, lde_up_idx);
    target_complex = sample_resampled_by_index(x_up, target_up_idx);
    phase_lde(k) = angle(lde_complex);
    phase_target(k) = angle(target_complex);
end

if ismember('Sequence', names)
    sequence = double(T.Sequence);
else
    sequence = (0:height(T)-1).';
end

P = struct();
P.sequence = sequence(:);
P.lde_abs_idx = lde_abs_idx(:);
P.lde_local_tap = lde_local_tap(:);
P.target_abs_idx = target_abs_idx(:);
P.target_local_tap = target_local_tap(:);
P.phase_lde = phase_lde(:);
P.phase_target = phase_target(:);
end

function v = sample_resampled_by_index(x_up, up_idx)
if isnan(up_idx) || up_idx < 1 || up_idx > numel(x_up)
    v = NaN + 1j * NaN;
    return;
end
v = x_up(up_idx);
end

function [aligned_idx1, aligned_idx2] = align_two_by_sequence(seq1, seq2)
if isempty(seq1) || isempty(seq2)
    aligned_idx1 = [];
    aligned_idx2 = [];
    return;
end

min_start_seq = min(seq1(1), seq2(1));
n_padded = max(numel(seq1), numel(seq2));
seq_range = mod(min_start_seq + (0:n_padded-1), 256);

seqs = {seq1(:), seq2(:)};
ptrs = [1, 1];
aligned = {[], []};

for i = 1:numel(seq_range)
    seq = seq_range(i);
    row_indices = nan(1, 2);

    for port_idx = 1:2
        port_seq = seqs{port_idx};
        ptr = ptrs(port_idx);

        if ptr <= numel(port_seq) && port_seq(ptr) == seq
            row_indices(port_idx) = ptr;
            ptr = ptr + 1;
        else
            while ptr <= numel(port_seq)
                seq_orig = port_seq(ptr);
                if seq_orig >= seq
                    forward_dist = seq_orig - seq;
                else
                    forward_dist = seq_orig - seq + 256;
                end

                if forward_dist >= 128
                    ptr = ptr + 1;
                else
                    break;
                end
            end
        end

        ptrs(port_idx) = ptr;
    end

    if all(~isnan(row_indices))
        aligned{1}(end+1, 1) = row_indices(1); %#ok<AGROW>
        aligned{2}(end+1, 1) = row_indices(2); %#ok<AGROW>
    end
end

aligned_idx1 = aligned{1};
aligned_idx2 = aligned{2};
end

function wrapped = wrap_phase(phase)
wrapped = angle(exp(1j * phase));
end

function mu = circular_mean(values)
values = values(~isnan(values));
if isempty(values)
    mu = NaN;
else
    mu = angle(mean(exp(1j * values)));
end
end

function sigma = circular_std(values)
values = values(~isnan(values));
if isempty(values)
    sigma = NaN;
    return;
end
R = abs(mean(exp(1j * values)));
R = min(max(R, eps), 1);
sigma = sqrt(-2 * log(R));
end

function print_phase_stats(label, values)
values = values(~isnan(values));
fprintf('\n%s\n', label);
fprintf('  Mean (circular): %.4f rad / %.2f deg\n', circular_mean(values), rad2deg(circular_mean(values)));
fprintf('  Std  (circular): %.4f rad / %.2f deg\n', circular_std(values), rad2deg(circular_std(values)));
fprintf('  Valid samples:   %d\n', numel(values));
end

function y_plot = break_wrap_jumps(y)
y_plot = y;
if numel(y_plot) < 2
    return;
end
jump_mask = [false; abs(diff(y_plot)) > pi];
y_plot(jump_mask) = NaN;
end

function plot_phase_results(frame_indices, rel1, rel2, dd, script_dir, port1_relative_offset, port2_relative_offset)
x = frame_indices(:);

fig = figure('Color', 'white', 'Position', [100, 100, 1200, 700]);
hold on;
plot(x, break_wrap_jumps(rel1), 'b-', 'LineWidth', 1.2, 'DisplayName', sprintf('Port 1: LDE+%.2f - LDE', port1_relative_offset));
plot(x, break_wrap_jumps(rel2), 'r-', 'LineWidth', 1.2, 'DisplayName', sprintf('Port 2: LDE+%.2f - LDE', port2_relative_offset));
plot(x, break_wrap_jumps(dd), 'k-', 'LineWidth', 1.8, 'DisplayName', 'Double difference');
grid on;
xlabel('Aligned frame index', 'FontSize', 16, 'FontWeight', 'bold');
ylabel('Phase (radians)', 'FontSize', 16, 'FontWeight', 'bold');
title('Cable Phase Fluctuation Referenced to LDE', 'FontSize', 18, 'FontWeight', 'bold');
legend('Location', 'best', 'FontSize', 13);
set(gca, 'FontSize', 14, 'LineWidth', 1.5);
ylim([-pi, pi]);

out_pdf = fullfile(script_dir, 'cable_phase_fluctuation_relative_tap.pdf');
out_png = fullfile(script_dir, 'cable_phase_fluctuation_relative_tap.png');
exportgraphics(fig, out_pdf, 'ContentType', 'vector', 'BackgroundColor', 'white');
exportgraphics(fig, out_png, 'Resolution', 300, 'BackgroundColor', 'white');
fprintf('Saved figure: %s\n', out_pdf);
fprintf('Saved figure: %s\n', out_png);

fig2 = figure('Color', 'white', 'Position', [150, 150, 900, 550]);
histogram(dd, 80, 'FaceColor', [0.1, 0.1, 0.1], 'EdgeColor', 'none');
grid on;
xlabel('Double difference phase (radians)', 'FontSize', 16, 'FontWeight', 'bold');
ylabel('Count', 'FontSize', 16, 'FontWeight', 'bold');
title(sprintf('Double Difference Distribution, circular std = %.4f rad', circular_std(dd)), ...
    'FontSize', 17, 'FontWeight', 'bold');
set(gca, 'FontSize', 14, 'LineWidth', 1.5);
xlim([-pi, pi]);

out_hist_pdf = fullfile(script_dir, 'cable_phase_fluctuation_relative_tap_hist.pdf');
out_hist_png = fullfile(script_dir, 'cable_phase_fluctuation_relative_tap_hist.png');
exportgraphics(fig2, out_hist_pdf, 'ContentType', 'vector', 'BackgroundColor', 'white');
exportgraphics(fig2, out_hist_png, 'Resolution', 300, 'BackgroundColor', 'white');
fprintf('Saved histogram: %s\n', out_hist_pdf);
fprintf('Saved histogram: %s\n', out_hist_png);
end
