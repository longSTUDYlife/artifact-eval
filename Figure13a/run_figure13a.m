% Figure 13(a): process env1 from raw CIR → errors + scatter.
% Raw: raw/ → MultiPort/20260209
%
% Default: load env1-fitted distance coefficients, then Stage 3 (MVDR).
% Optional: FORCE_REFIT=true  → Stage 1 collect + Stage 2 fit, then Stage 3
% Optional: SMOKE=true        → only 0° / 2 m
script_dir = fileparts(mfilename('fullpath'));
cd(script_dir);
addpath(script_dir);
set(0, 'DefaultFigureVisible', 'off');

fprintf('========================================\n');
fprintf('  Figure 13(a) env1 from raw (8RX)\n');
fprintf('========================================\n\n');

phase_compensation_8port_deg = [
    0.00
    471.1
    239.1
    308.3
    208.6
    124.3
    409.2
    660.7
];
phase_compensation_8port_deg_1m = [
    0.00
    -269.7
    -175.6
    -105.0
    -209.4
    51.8
    -10.1
    -116.2
];
phase_compensation_8port = phase_compensation_8port_deg * pi/180;
phase_compensation_8port_1m = phase_compensation_8port_deg_1m * pi/180;
fprintf('Phase calib: env1 (>1 m) / env1_1m (=1 m)\n');
fprintf('  >1m: '); fprintf('%.1f ', phase_compensation_8port_deg); fprintf('\n');
fprintf('  1m:  '); fprintf('%.1f ', phase_compensation_8port_deg_1m); fprintf('\n\n');

smoke = false;
if exist('SMOKE', 'var') && SMOKE
    smoke = true;
end
force_refit = false;
if exist('FORCE_REFIT', 'var') && FORCE_REFIT
    force_refit = true;
end

if smoke
    test_angles = {'0'};
    test_distances = {'2'};
    fprintf('SMOKE mode: angle=0, dist=2m only\n');
else
    test_angles = {'-40','-30','-20','-10','0','10','20','30','40'};
    test_distances = {'1','2','3','4'};
end

aoa_method = 'mvdr';
coeff_file = fullfile(script_dir, 'distance_error_correction_coefficients.csv');

%% Distance correction coefficients (env1 fit, or refit from raw)
if force_refit || ~exist(coeff_file, 'file')
    fprintf('===== Stage 1: collect (no distance correction) =====\n');
    all_fit_data = struct('measured_dist', [], 'true_dist', [], 'angle', []);
    for i = 1:numel(test_angles)
        for j = 1:numel(test_distances)
            a = test_angles{i}; d = test_distances{j};
            pc = phase_for_distance(d, phase_compensation_8port, phase_compensation_8port_1m);
            result = test_single_angle_distance_multi_config_v3( ...
                a, d, pc, 8, 1:8, aoa_method, false, [], false);
            if ~isempty(result) && isfield(result, 'fit_data')
                all_fit_data.measured_dist = [all_fit_data.measured_dist; result.fit_data.measured_dist]; %#ok<AGROW>
                all_fit_data.true_dist = [all_fit_data.true_dist; result.fit_data.true_dist]; %#ok<AGROW>
                all_fit_data.angle = [all_fit_data.angle; result.fit_data.angle]; %#ok<AGROW>
            end
        end
    end
    fprintf('Collected %d fit points\n', numel(all_fit_data.measured_dist));

    fprintf('\n===== Stage 2: fit distance correction =====\n');
    errors_fit = all_fit_data.measured_dist - all_fit_data.true_dist;
    nfit = numel(errors_fit);
    A = [ones(nfit,1), all_fit_data.angle, all_fit_data.true_dist, ...
         all_fit_data.angle.^2, all_fit_data.true_dist.^2, ...
         all_fit_data.angle .* all_fit_data.true_dist];
    coeffs = pinv(A) * errors_fit;
    new_correction_coefficients = struct( ...
        'const', coeffs(1), 'angle', coeffs(2), 'dist', coeffs(3), ...
        'angle2', coeffs(4), 'dist2', coeffs(5), 'angle_dist', coeffs(6));
    pred = A * coeffs;
    ss_res = sum((errors_fit - pred).^2);
    ss_tot = sum((errors_fit - mean(errors_fit)).^2);
    fprintf('Fitted: R²=%.4f RMSE=%.4f m\n', 1 - ss_res/ss_tot, sqrt(mean((errors_fit - pred).^2)));
    fprintf('  Error = %.6f + %.6f*A + %.6f*D + %.6f*A² + %.6f*D² + %.6f*A*D\n\n', ...
        coeffs(1), coeffs(2), coeffs(3), coeffs(4), coeffs(5), coeffs(6));
    if exist(coeff_file, 'file')
        copyfile(coeff_file, fullfile(script_dir, 'distance_error_correction_coefficients_prev.csv'));
    end
    writetable(struct2table(new_correction_coefficients), coeff_file);
    fprintf('Saved %s\n', coeff_file);
else
    T = readtable(coeff_file);
    new_correction_coefficients = struct( ...
        'const', T.const(1), 'angle', T.angle(1), 'dist', T.dist(1), ...
        'angle2', T.angle2(1), 'dist2', T.dist2(1), 'angle_dist', T.angle_dist(1));
    fprintf('Loaded distance correction from %s\n', coeff_file);
    fprintf('  Error = %.6f + %.6f*A + %.6f*D + %.6f*A² + %.6f*D² + %.6f*A*D\n\n', ...
        new_correction_coefficients.const, new_correction_coefficients.angle, ...
        new_correction_coefficients.dist, new_correction_coefficients.angle2, ...
        new_correction_coefficients.dist2, new_correction_coefficients.angle_dist);
end

%% Stage 3: 8-port MVDR + correction + RMSE filter, from raw / lde_cache
fprintf('===== Stage 3: 8-port MVDR from raw =====\n');
results = [];
for i = 1:numel(test_angles)
    for j = 1:numel(test_distances)
        a = test_angles{i}; d = test_distances{j};
        if strcmp(d, '1')
            calib_tag = 'env1_1m';
        else
            calib_tag = 'env1';
        end
        pc = phase_for_distance(d, phase_compensation_8port, phase_compensation_8port_1m);
        r = test_single_angle_distance_multi_config_v3( ...
            a, d, pc, 8, 1:8, aoa_method, true, new_correction_coefficients, true);
        if isempty(r)
            fprintf('  SKIP %s° %sm\n', a, d);
            continue;
        end
        results = [results; r]; %#ok<AGROW>
        e = sqrt((r.estimated_x_cal - r.true_x).^2 + (r.estimated_y_cal - r.true_y).^2);
        e = e(~isnan(e));
        fprintf('  [%s] %s°/%sm: N=%d mean|err|=%.3f m  AoA=%.1f°\n', ...
            calib_tag, a, d, numel(e), mean(e), r.aoa_cal_mean);
    end
end

all_err = [];
for k = 1:numel(results)
    e = sqrt((results(k).estimated_x_cal - results(k).true_x).^2 + ...
             (results(k).estimated_y_cal - results(k).true_y).^2);
    e = e(~isnan(e));
    all_err = [all_err; e(:)]; %#ok<AGROW>
end

n = numel(all_err);
med = median(all_err);
p90 = prctile(all_err, 90);
rmse = sqrt(mean(all_err.^2));
fprintf('\nN = %d, median = %.3f m, 90th = %.3f m, RMSE = %.3f m\n', n, med, p90, rmse);

out_err = fullfile(script_dir, 'localization_errors_8port.csv');
writetable(table(all_err, 'VariableNames', {'Localization_Error'}), out_err);
fprintf('Saved %s\n', out_err);

out_scatter = fullfile(script_dir, 'localization_scatter_data.csv');
save_localization_scatter(results, out_scatter);

fprintf('\nPlot: python plot_figure13a.py\n');
py_candidates = { ...
    fullfile(script_dir, '..', '.venv_pack', 'bin', 'python'), ...
    fullfile(script_dir, '..', 'Done', '.venv_pack', 'bin', 'python'), ...
    fullfile(script_dir, '..', 'Figure10', '.venv_plot', 'bin', 'python'), ...
    'python3'};
for pi = 1:numel(py_candidates)
    py = py_candidates{pi};
    if strcmp(py, 'python3') || exist(py, 'file')
        cmd = sprintf('"%s" "%s"', py, fullfile(script_dir, 'plot_figure13a.py'));
        fprintf('Running: %s\n', cmd);
        system(cmd);
        break;
    end
end

fprintf('\nDONE\n');

function pc = phase_for_distance(d, phase_gt1m, phase_1m)
if strcmp(d, '1')
    pc = phase_1m;
else
    pc = phase_gt1m;
end
end

function save_localization_scatter(results_to_save, out_csv)
% Per cell: 1 GND + top-50 EST by localization error (same as test_all v3).
n_points_per_position = 50;
if isempty(results_to_save)
    fprintf('  No results for scatter plot\n');
    return;
end

all_angles = [results_to_save.angle]';
if isfield(results_to_save, 'distance_from_filename')
    all_distances = [results_to_save.distance_from_filename]';
else
    all_distances = round([results_to_save.distance]');
end
unique_angle_dist = unique([all_angles, all_distances], 'rows');

true_angles = [];
true_distances = [];
estimated_angles = [];
estimated_distances = [];

fprintf('===== Saving scatter (top %d EST per cell) =====\n', n_points_per_position);
for i = 1:size(unique_angle_dist, 1)
    angle = unique_angle_dist(i, 1);
    distance = unique_angle_dist(i, 2);
    if isfield(results_to_save, 'distance_from_filename')
        idx = find([results_to_save.angle] == angle & [results_to_save.distance_from_filename] == distance);
    else
        idx = find([results_to_save.angle] == angle & round([results_to_save.distance]) == distance);
    end
    if isempty(idx)
        continue;
    end
    result = results_to_save(idx(1));
    true_angles = [true_angles; result.angle]; %#ok<AGROW>
    true_distances = [true_distances; distance]; %#ok<AGROW>

    est_x = result.estimated_x_cal;
    est_y = result.estimated_y_cal;
    valid_est = ~isnan(est_x) & ~isnan(est_y);
    if ~any(valid_est)
        continue;
    end
    true_x = distance * cos(deg2rad(result.angle));
    true_y = distance * sin(deg2rad(result.angle));
    frame_errors = sqrt((est_x - true_x).^2 + (est_y - true_y).^2);
    valid_mask = ~isnan(frame_errors) & valid_est;
    frame_errors = frame_errors(valid_mask);
    est_x_valid = est_x(valid_mask);
    est_y_valid = est_y(valid_mask);
    if isempty(frame_errors)
        continue;
    end
    [~, sort_idx] = sort(frame_errors, 'ascend');
    n_select = min(n_points_per_position, numel(frame_errors));
    selected_idx = sort_idx(1:n_select);
    selected_x = est_x_valid(selected_idx);
    selected_y = est_y_valid(selected_idx);
    selected_angles = rad2deg(atan2(selected_y, selected_x));
    selected_distances = sqrt(selected_x.^2 + selected_y.^2);
    estimated_angles = [estimated_angles; selected_angles(:)]; %#ok<AGROW>
    estimated_distances = [estimated_distances; selected_distances(:)]; %#ok<AGROW>
    fprintf('    Angle=%.1f°, Distance=%.1fm: Selected %d EST (from %d valid)\n', ...
        angle, distance, n_select, numel(frame_errors));
end

if ~isempty(true_angles)
    [~, unique_gnd_idx] = unique([true_angles, true_distances], 'rows', 'stable');
    true_angles = true_angles(unique_gnd_idx);
    true_distances = true_distances(unique_gnd_idx);
end

T_scatter = table( ...
    [true_angles; estimated_angles], ...
    [true_distances; estimated_distances], ...
    [ones(numel(true_angles), 1); zeros(numel(estimated_angles), 1)], ...
    'VariableNames', {'angle', 'distance', 'type'});
writetable(T_scatter, out_csv);
fprintf('  Saved %s (%d GND + %d EST)\n', out_csv, numel(true_angles), numel(estimated_angles));
end
