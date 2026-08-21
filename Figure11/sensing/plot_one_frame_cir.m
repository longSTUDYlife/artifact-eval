function plot_one_frame_cir()
% Plot one frame of cable CIR for sanity-checking LDE index and target tap.

script_dir = fileparts(mfilename('fullpath'));
port1_csv = fullfile(script_dir, 'cable_data_port1_fp_mp.csv');
port2_csv = fullfile(script_dir, 'cable_data_port2_fp_mp.csv');

frame_idx = 1;   % Change this to inspect another row in each CSV.
first_cir_tap = 699;
port1_relative_offset = 1584 / 64;
port2_relative_offset = 1778 / 64;

P1 = load_one_port_cir(port1_csv, frame_idx, first_cir_tap, port1_relative_offset);
P2 = load_one_port_cir(port2_csv, frame_idx, first_cir_tap, port2_relative_offset);

fig = figure('Color', 'white', 'Position', [100, 100, 1200, 650]);

subplot(2, 1, 1);
plot_cir_frame(P1, sprintf('Port 1, row %d, Sequence %g', frame_idx, P1.sequence));

subplot(2, 1, 2);
plot_cir_frame(P2, sprintf('Port 2, row %d, Sequence %g', frame_idx, P2.sequence));

out_pdf = fullfile(script_dir, sprintf('one_frame_cir_row%d_relative_tap.pdf', frame_idx));
out_png = fullfile(script_dir, sprintf('one_frame_cir_row%d_relative_tap.png', frame_idx));
exportgraphics(fig, out_pdf, 'ContentType', 'vector', 'BackgroundColor', 'white');
exportgraphics(fig, out_png, 'Resolution', 300, 'BackgroundColor', 'white');

fprintf('Saved: %s\n', out_pdf);
fprintf('Saved: %s\n', out_png);
fprintf('Port 1 firstPath abs index=%.6f, LDE local=%.6f, target local=%.6f\n', P1.first_path_raw, P1.lde_local_tap, P1.target_local_tap);
fprintf('Port 2 firstPath abs index=%.6f, LDE local=%.6f, target local=%.6f\n', P2.first_path_raw, P2.lde_local_tap, P2.target_local_tap);
end

function P = load_one_port_cir(csv_file, frame_idx, first_cir_tap, relative_offset)
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

if frame_idx < 1 || frame_idx > height(T)
    error('frame_idx=%d is outside table height %d', frame_idx, height(T));
end

cir = complex(double(T{frame_idx, real_cols}), double(T{frame_idx, imag_cols}));
first_path = double(T.firstPath(frame_idx));
nbins = numel(cir);
abs_bins = first_cir_tap + (0:nbins-1);
lde_abs_idx = first_path;
lde_local_tap = lde_abs_idx - first_cir_tap + 1;
target_abs_idx = lde_abs_idx + relative_offset;
target_local_tap = lde_local_tap + relative_offset;

if ismember('Sequence', names)
    sequence = double(T.Sequence(frame_idx));
else
    sequence = frame_idx - 1;
end

P = struct();
P.cir = cir;
P.mag = abs(cir);
P.local_bins = real_idx_sorted(:).';
P.abs_bins = abs_bins;
P.first_path_raw = first_path;
P.lde_abs_idx = lde_abs_idx;
P.lde_local_tap = lde_local_tap;
P.target_abs_idx = target_abs_idx;
P.target_local_tap = target_local_tap;
P.relative_offset = relative_offset;
P.sequence = sequence;
end

function plot_cir_frame(P, title_text)
plot(P.abs_bins, P.mag, 'b-', 'LineWidth', 1.5);
hold on;
grid on;

xline(P.lde_abs_idx, 'r--', 'LineWidth', 2.0, ...
    'Label', sprintf('LDE abs %.2f', P.lde_abs_idx), ...
    'LabelOrientation', 'horizontal', 'LabelVerticalAlignment', 'bottom');
xline(P.target_abs_idx, 'k--', 'LineWidth', 2.0, ...
    'Label', sprintf('LDE+%.2f abs %.2f', P.relative_offset, P.target_abs_idx), ...
    'LabelOrientation', 'horizontal', 'LabelVerticalAlignment', 'top');

if P.lde_abs_idx >= min(P.abs_bins) && P.lde_abs_idx <= max(P.abs_bins)
    lde_mag = abs(sample_cir_upsampled(P.cir, P.lde_local_tap));
    plot(P.lde_abs_idx, lde_mag, 'ro', 'MarkerFaceColor', 'r', 'MarkerSize', 8);
end
if P.target_abs_idx >= min(P.abs_bins) && P.target_abs_idx <= max(P.abs_bins)
    target_mag = abs(sample_cir_upsampled(P.cir, P.target_local_tap));
    plot(P.target_abs_idx, target_mag, 'ko', 'MarkerFaceColor', 'k', 'MarkerSize', 8);
end

xlabel('Absolute CIR index', 'FontSize', 13, 'FontWeight', 'bold');
ylabel('|CIR|', 'FontSize', 13, 'FontWeight', 'bold');
title(title_text, 'FontSize', 15, 'FontWeight', 'bold');
set(gca, 'FontSize', 12, 'LineWidth', 1.2);
end

function v = sample_cir_upsampled(x, local_tap)
L = 64;
nbins = numel(x);
if isnan(local_tap) || local_tap < 1 || local_tap > nbins
    v = NaN + 1j * NaN;
    return;
end
xi = 1 : 1/L : nbins;
x_up = interp1(1:nbins, x, xi, 'pchip', 'extrap');
idx = min(max(round((local_tap - 1) * L) + 1, 1), numel(x_up));
v = x_up(idx);
end
