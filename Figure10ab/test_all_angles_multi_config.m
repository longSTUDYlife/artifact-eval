% Test all angles with multiple antenna configurations (8, 4, 2 ports)
% User can select which configurations to plot

fprintf('========================================\n');
fprintf('   Multi-Angle AoA Test (Multi-Config)\n');
fprintf('   Figure 10ab batch (non-interactive)\n');
fprintf('========================================\n\n');

% Stay in Figure10a so exported CSVs land next to the plot script
script_dir = fileparts(mfilename('fullpath'));
cd(script_dir);
addpath(script_dir);
set(0, 'DefaultFigureVisible', 'off');

% Pre-computed phase compensation for different configurations
% From browse_fft_robustness_0.m

 % Configuration 1: 8-port (1-8)
% phase_compensation_8port_deg = [
%      0.00;     % Port 1
%      -117.2;  % Port 2
%      -256.4;    % Port 3
%      -24.6;  % Port 4
%      -520.7;  % Port 5
%      -143.7;    % Port 6
%      -198.5;   % Port 7
%      251.6    % Port 8
% ];
% best
phase_compensation_8port_deg = [
    0.00;     % Port 1
    120.9;  % Port 2
    -109.4;    % Port 3
    350.2;  % Port 4
    441.6;  % Port 5
    352.2;    % Port 6
    196.1;   % Port 7
    44.8    % Port 8
];
% phase_compensation_8port_deg = [
%     0.00;     % Port 1
%     -180.5;  % Port 2
%     -30.4;    % Port 3
%     103.9;  % Port 4
%     -234.9;  % Port 5
%     -379.8;    % Port 6
%     -198.1;   % Port 7
%     -345.1    % Port 8
% ];
%good
% phase_compensation_8port_deg = [
%     0.00;     % Port 1
%     -311.89;  % Port 2
%     -104.10;    % Port 3
%     -89.91;  % Port 4
%     98.06;  % Port 5
%     0.01;    % Port 6
%     143.38;   % Port 7
%     362.72    % Port 8
% ];
% phase_compensation_8port_deg = [
%     0.00;     % Port 1
%     64.76;  % Port 2
%     -17.81;    % Port 3
%     -58.35;  % Port 4
%     22.37;  % Port 5
%     -76.01;    % Port 6
%     167.13;   % Port 7
%     -14.93    % Port 8
% ];

% Configuration 2: 4-port (3-6), Port 3 is reference
%best
 phase_compensation_4port_deg = [
     0;    % Port 3
    350.2+109.4;  % Port 4
    441.6+109.4;  % Port 5
    352.2+109.4;    % Port 6
 ];
 % phase_compensation_4port_deg = [
 %     0;    % Port 3
 %     54.44;  % Port 4
 %     147.57;  % Port 5
 %     103.63;    % Port 6
 % ];
% good Configuration 2: 4-port (3-6), Port 3 is reference
% phase_compensation_4port_deg = [
%     0.00;     % Port 3 (reference)
%     73.07;  % Port 4
%     191.51;  % Port 5
%     85.39     % Port 6
% ];
% phase_compensation_4port_deg = [
%     0.00;     % Port 3 (reference)
%     8.62;  % Port 4
%     82.83;  % Port 5
%     89.99     % Port 6
% ];

% good Configuration 3: 2-port (4-5), Port 4 is reference
% phase_compensation_2port_deg = [
%     0;  % Port 4
%     441.6-350.2;  % Port 5
% ];
phase_compensation_2port_deg = [
    0;    % Port 3
    350.2+109.4;  
];
% phase_compensation_2port_deg = [
%      0;  % Port 4
%      93.16;  % Port 5
% ];

phase_compensation_8port = phase_compensation_8port_deg * pi/180;
phase_compensation_4port = phase_compensation_4port_deg * pi/180;
phase_compensation_2port = phase_compensation_2port_deg * pi/180;

fprintf('Phase compensation values:\n');
fprintf('  8-port (1-8):\n');
for port = 1:8
    fprintf('    Port %d: %.2f°\n', port, phase_compensation_8port_deg(port));
end
fprintf('  4-port (3-6):\n');
fprintf('    Port 3: %.2f° (reference)\n', phase_compensation_4port_deg(1));
fprintf('    Port 4: %.2f°\n', phase_compensation_4port_deg(2));
fprintf('    Port 5: %.2f°\n', phase_compensation_4port_deg(3));
fprintf('    Port 6: %.2f°\n', phase_compensation_4port_deg(4));
fprintf('  2-port (4-5):\n');
fprintf('    Port 4: %.2f° (reference)\n', phase_compensation_2port_deg(1));
fprintf('    Port 5: %.2f°\n', phase_compensation_2port_deg(2));
fprintf('\n');

%% Non-interactive settings for Figure 10a Localization CDF
% Method: MVDR; IQR / unify / etc. all off
selected_configs = [1, 2, 3];  % 8-port, 4-port, 2-port
aoa_method = 'mvdr';
enable_iqr_filter = false;
filter_method = 'none';
unify_frame_count = false;

fprintf('Selected configurations: ');
config_names = {'8-port (cal)', '4-port (cal)', '2-port (cal)', 'Uncalibrated'};
for i = 1:length(selected_configs)
    fprintf('%s ', config_names{selected_configs(i)});
end
fprintf('\n');
fprintf('Selected method: %s\n', upper(strrep(aoa_method, '_', ' ')));
fprintf('IQR filtering: Disabled\n');
fprintf('Unify frame count: Disabled\n\n');

%% IQR params unused when filtering disabled
iqr_upper_multiplier = 5;
iqr_lower_multiplier = 1;

%% Test angles
test_angles = {'-40','-30','-20', '-10', '0','10','20','30','40'};

%% Process each configuration
results_8port = [];
results_4port = [];
results_2port = [];
results_uncal = [];

% Process 8-port if selected
if ismember(1, selected_configs) || ismember(4, selected_configs)
    method_display = upper(strrep(aoa_method, '_', ' '));
    fprintf('Processing 8-port configuration (ports 1-8, method: %s)...\n', method_display);
    for i = 1:length(test_angles)
        angle_str = test_angles{i};
        result = test_single_angle_multi_config_v2(angle_str, phase_compensation_8port, 8, 1:8, aoa_method);
        if ~isempty(result)
            if ismember(1, selected_configs)
                results_8port = [results_8port; result]; %#ok<AGROW>
            end
            if ismember(4, selected_configs)
                results_uncal = [results_uncal; result]; %#ok<AGROW>
            end
        end
    end
end

% Process 4-port if selected
if ismember(2, selected_configs)
    method_display = upper(strrep(aoa_method, '_', ' '));
    fprintf('\nProcessing 4-port configuration (ports 3-6, method: %s)...\n', method_display);
    for i = 1:length(test_angles)
        angle_str = test_angles{i};
        result = test_single_angle_multi_config_v2(angle_str, phase_compensation_4port, 4, 3:6, aoa_method);
        if ~isempty(result)
            results_4port = [results_4port; result]; %#ok<AGROW>
        end
    end
end

% Process 2-port if selected
if ismember(3, selected_configs)
    method_display = upper(strrep(aoa_method, '_', ' '));
    fprintf('\nProcessing 2-port configuration (ports 4-5, method: %s)...\n', method_display);
    for i = 1:length(test_angles)
        angle_str = test_angles{i};
        result = test_single_angle_multi_config_v2(angle_str, phase_compensation_2port, 2, 3:4, aoa_method);
        if ~isempty(result)
            results_2port = [results_2port; result]; %#ok<AGROW>
        end
    end
end

%% Collect per-frame errors for each configuration
% Also track frame indices for each angle to compute per-angle filtered stats
all_frame_errors_8port = [];
all_frame_errors_4port = [];
all_frame_errors_2port = [];
all_frame_errors_uncal = [];

% Track frame ranges for each angle (for filtering)
angle_frame_ranges_8port = cell(length(results_8port), 1);
angle_frame_ranges_4port = cell(length(results_4port), 1);
angle_frame_ranges_2port = cell(length(results_2port), 1);
angle_valid_masks_8port = cell(length(results_8port), 1);
angle_valid_masks_4port = cell(length(results_4port), 1);
angle_valid_masks_2port = cell(length(results_2port), 1);

frame_start_idx = 1;
if ~isempty(results_8port)
    for i = 1:length(results_8port)
        true_angle = results_8port(i).angle;
        frame_errors = abs(results_8port(i).aoa_cal - true_angle);
        valid_mask = ~isnan(frame_errors);
        angle_valid_masks_8port{i} = valid_mask;
        frame_errors = frame_errors(valid_mask);
        n_frames = length(frame_errors);
        angle_frame_ranges_8port{i} = frame_start_idx:(frame_start_idx + n_frames - 1);
        frame_start_idx = frame_start_idx + n_frames;
        all_frame_errors_8port = [all_frame_errors_8port; frame_errors(:)]; %#ok<AGROW>
    end
end

frame_start_idx = 1;
if ~isempty(results_4port)
    for i = 1:length(results_4port)
        true_angle = results_4port(i).angle;
        frame_errors = abs(results_4port(i).aoa_cal - true_angle);
        valid_mask = ~isnan(frame_errors);
        angle_valid_masks_4port{i} = valid_mask;
        frame_errors = frame_errors(valid_mask);
        n_frames = length(frame_errors);
        angle_frame_ranges_4port{i} = frame_start_idx:(frame_start_idx + n_frames - 1);
        frame_start_idx = frame_start_idx + n_frames;
        all_frame_errors_4port = [all_frame_errors_4port; frame_errors(:)]; %#ok<AGROW>
    end
end

frame_start_idx = 1;
if ~isempty(results_2port)
    for i = 1:length(results_2port)
        true_angle = results_2port(i).angle;
        frame_errors = abs(results_2port(i).aoa_cal - true_angle);
        valid_mask = ~isnan(frame_errors);
        angle_valid_masks_2port{i} = valid_mask;
        frame_errors = frame_errors(valid_mask);
        n_frames = length(frame_errors);
        angle_frame_ranges_2port{i} = frame_start_idx:(frame_start_idx + n_frames - 1);
        frame_start_idx = frame_start_idx + n_frames;
        all_frame_errors_2port = [all_frame_errors_2port; frame_errors(:)]; %#ok<AGROW>
    end
end

if ~isempty(results_uncal)
    for i = 1:length(results_uncal)
        true_angle = results_uncal(i).angle;
        frame_errors = abs(results_uncal(i).aoa_uncal - true_angle);
        frame_errors = frame_errors(~isnan(frame_errors));
        all_frame_errors_uncal = [all_frame_errors_uncal; frame_errors(:)]; %#ok<AGROW>
    end
end

%% Outlier Detection and Filtering (IQR method)
% Get all unique angles from all configs (needed for both IQR and non-IQR paths)
all_angles = [];
if ~isempty(results_8port)
    all_angles = [all_angles, [results_8port.angle]];
end
if ~isempty(results_4port)
    all_angles = [all_angles, [results_4port.angle]];
end
if ~isempty(results_2port)
    all_angles = [all_angles, [results_2port.angle]];
end
all_angles = unique(all_angles);

if enable_iqr_filter
    % IQR filtering enabled
    if strcmp(filter_method, 'global')
        % Filter each configuration globally (across all angles), then use intersection of filtered frames
        fprintf('\n===== Outlier Detection (IQR method, global per-config then intersection) =====\n');
        
        % Filter 8-port calibrated errors (global, across all angles)
        outlier_mask_8port = [];
        n_outliers_8port = 0;
        if ~isempty(all_frame_errors_8port)
            Q1_8port = prctile(all_frame_errors_8port, 25);
            Q3_8port = prctile(all_frame_errors_8port, 75);
            IQR_8port = Q3_8port - Q1_8port;
            upper_bound_8port = Q3_8port + iqr_upper_multiplier * IQR_8port;
            lower_bound_8port = max(0, Q1_8port - iqr_lower_multiplier * IQR_8port);
            outlier_mask_8port = (all_frame_errors_8port > upper_bound_8port) | (all_frame_errors_8port < lower_bound_8port);
            n_outliers_8port = sum(outlier_mask_8port);
            fprintf('  8-port (cal): Q1=%.2f°, Q3=%.2f°, IQR=%.2f°, Upper=%.2f°, Outliers: %d (%.1f%%)\n', ...
                Q1_8port, Q3_8port, IQR_8port, upper_bound_8port, n_outliers_8port, 100*n_outliers_8port/length(all_frame_errors_8port));
        end
        
        % Filter 4-port calibrated errors (global, across all angles)
        outlier_mask_4port = [];
        n_outliers_4port = 0;
        if ~isempty(all_frame_errors_4port)
            Q1_4port = prctile(all_frame_errors_4port, 25);
            Q3_4port = prctile(all_frame_errors_4port, 75);
            IQR_4port = Q3_4port - Q1_4port;
            upper_bound_4port = Q3_4port + iqr_upper_multiplier * IQR_4port;
            lower_bound_4port = max(0, Q1_4port - iqr_lower_multiplier * IQR_4port);
            outlier_mask_4port = (all_frame_errors_4port > upper_bound_4port) | (all_frame_errors_4port < lower_bound_4port);
            n_outliers_4port = sum(outlier_mask_4port);
            fprintf('  4-port (cal): Q1=%.2f°, Q3=%.2f°, IQR=%.2f°, Upper=%.2f°, Outliers: %d (%.1f%%)\n', ...
                Q1_4port, Q3_4port, IQR_4port, upper_bound_4port, n_outliers_4port, 100*n_outliers_4port/length(all_frame_errors_4port));
        end
        
        % Filter 2-port calibrated errors (global, across all angles)
        outlier_mask_2port = [];
        n_outliers_2port = 0;
        if ~isempty(all_frame_errors_2port)
            Q1_2port = prctile(all_frame_errors_2port, 25);
            Q3_2port = prctile(all_frame_errors_2port, 75);
            IQR_2port = Q3_2port - Q1_2port;
            upper_bound_2port = Q3_2port + iqr_upper_multiplier * IQR_2port;
            lower_bound_2port = max(0, Q1_2port - iqr_lower_multiplier * IQR_2port);
            outlier_mask_2port = (all_frame_errors_2port > upper_bound_2port) | (all_frame_errors_2port < lower_bound_2port);
            n_outliers_2port = sum(outlier_mask_2port);
            fprintf('  2-port (cal): Q1=%.2f°, Q3=%.2f°, IQR=%.2f°, Upper=%.2f°, Outliers: %d (%.1f%%)\n', ...
                Q1_2port, Q3_2port, IQR_2port, upper_bound_2port, n_outliers_2port, 100*n_outliers_2port/length(all_frame_errors_2port));
        end
        
        % For each angle, find intersection of filtered frames from all configs
        % Store per-angle filtered frame indices for each config (in original aoa_cal array)
        angle_filtered_indices_8port = cell(length(results_8port), 1);
        angle_filtered_indices_4port = cell(length(results_4port), 1);
        angle_filtered_indices_2port = cell(length(results_2port), 1);
        
        if ~isempty(results_8port) && ~isempty(outlier_mask_8port)
            for i = 1:length(results_8port)
                frame_idx_8port = angle_frame_ranges_8port{i};
                angle_outlier_mask_8port = outlier_mask_8port(frame_idx_8port);
                angle_filtered_indices_8port{i} = find(~angle_outlier_mask_8port);
            end
        end
        
        if ~isempty(results_4port) && ~isempty(outlier_mask_4port)
            for i = 1:length(results_4port)
                frame_idx_4port = angle_frame_ranges_4port{i};
                angle_outlier_mask_4port = outlier_mask_4port(frame_idx_4port);
                angle_filtered_indices_4port{i} = find(~angle_outlier_mask_4port);
            end
        end
        
        if ~isempty(results_2port) && ~isempty(outlier_mask_2port)
            for i = 1:length(results_2port)
                frame_idx_2port = angle_frame_ranges_2port{i};
                angle_outlier_mask_2port = outlier_mask_2port(frame_idx_2port);
                angle_filtered_indices_2port{i} = find(~angle_outlier_mask_2port);
            end
        end
        
    else
        % Filter each angle separately for each config, then use intersection of filtered frames
        fprintf('\n===== Outlier Detection (IQR method, per-angle then intersection) =====\n');
        
        % Store per-angle filtered frame indices for each config (in original aoa_cal array)
        if ~isempty(results_8port)
            angle_filtered_indices_8port = cell(length(results_8port), 1);
        else
            angle_filtered_indices_8port = {};
        end
        if ~isempty(results_4port)
            angle_filtered_indices_4port = cell(length(results_4port), 1);
        else
            angle_filtered_indices_4port = {};
        end
        if ~isempty(results_2port)
            angle_filtered_indices_2port = cell(length(results_2port), 1);
        else
            angle_filtered_indices_2port = {};
        end
        
        % For each angle, perform IQR filtering per config, then find intersection
        for angle = all_angles(:)'
            % Find indices for this angle in each config
            idx_8port = [];
            if ~isempty(results_8port)
                idx_8port = find([results_8port.angle] == angle, 1);
            end
            idx_4port = [];
            if ~isempty(results_4port)
                idx_4port = find([results_4port.angle] == angle, 1);
            end
            idx_2port = [];
            if ~isempty(results_2port)
                idx_2port = find([results_2port.angle] == angle, 1);
            end
            
            % Filter 8-port for this angle
            if ~isempty(idx_8port)
                frame_idx_8port = angle_frame_ranges_8port{idx_8port};
                angle_errors_8port = all_frame_errors_8port(frame_idx_8port);
                if ~isempty(angle_errors_8port) && length(angle_errors_8port) > 3  % Need at least 4 points for IQR
                    Q1 = prctile(angle_errors_8port, 25);
                    Q3 = prctile(angle_errors_8port, 75);
                    IQR = Q3 - Q1;
                    if IQR > 0
                        upper_bound = Q3 + iqr_upper_multiplier * IQR;
                        lower_bound = max(0, Q1 - iqr_lower_multiplier * IQR);
                        angle_outlier_mask = (angle_errors_8port > upper_bound) | (angle_errors_8port < lower_bound);
                        angle_filtered_indices_8port{idx_8port} = find(~angle_outlier_mask);
                    else
                        angle_filtered_indices_8port{idx_8port} = 1:length(angle_errors_8port);
                    end
                else
                    angle_filtered_indices_8port{idx_8port} = 1:length(frame_idx_8port);
                end
            end
            
            % Filter 4-port for this angle
            if ~isempty(idx_4port)
                frame_idx_4port = angle_frame_ranges_4port{idx_4port};
                angle_errors_4port = all_frame_errors_4port(frame_idx_4port);
                if ~isempty(angle_errors_4port) && length(angle_errors_4port) > 3  % Need at least 4 points for IQR
                    Q1 = prctile(angle_errors_4port, 25);
                    Q3 = prctile(angle_errors_4port, 75);
                    IQR = Q3 - Q1;
                    if IQR > 0
                        upper_bound = Q3 + iqr_upper_multiplier * IQR;
                        lower_bound = max(0, Q1 - iqr_lower_multiplier * IQR);
                        angle_outlier_mask = (angle_errors_4port > upper_bound) | (angle_errors_4port < lower_bound);
                        angle_filtered_indices_4port{idx_4port} = find(~angle_outlier_mask);
                    else
                        angle_filtered_indices_4port{idx_4port} = 1:length(angle_errors_4port);
                    end
                else
                    angle_filtered_indices_4port{idx_4port} = 1:length(frame_idx_4port);
                end
            end
            
            % Filter 2-port for this angle
            if ~isempty(idx_2port)
                frame_idx_2port = angle_frame_ranges_2port{idx_2port};
                angle_errors_2port = all_frame_errors_2port(frame_idx_2port);
                if ~isempty(angle_errors_2port) && length(angle_errors_2port) > 3  % Need at least 4 points for IQR
                    Q1 = prctile(angle_errors_2port, 25);
                    Q3 = prctile(angle_errors_2port, 75);
                    IQR = Q3 - Q1;
                    if IQR > 0
                        upper_bound = Q3 + iqr_upper_multiplier * IQR;
                        lower_bound = max(0, Q1 - iqr_lower_multiplier * IQR);
                        angle_outlier_mask = (angle_errors_2port > upper_bound) | (angle_errors_2port < lower_bound);
                        angle_filtered_indices_2port{idx_2port} = find(~angle_outlier_mask);
                    else
                        angle_filtered_indices_2port{idx_2port} = 1:length(angle_errors_2port);
                    end
                else
                    angle_filtered_indices_2port{idx_2port} = 1:length(frame_idx_2port);
                end
            end
        end
        
        fprintf('  Per-angle IQR filtering completed for all angles\n');
    end
else
    % IQR filtering disabled: use all valid frames, then intersection
    fprintf('\n===== Skipping IQR Filtering (using all valid frames, then intersection) =====\n');
    
    % Store per-angle frame indices for each config (using all valid frames, no IQR filtering)
    angle_filtered_indices_8port = cell(length(results_8port), 1);
    angle_filtered_indices_4port = cell(length(results_4port), 1);
    angle_filtered_indices_2port = cell(length(results_2port), 1);
    
    % For each angle, use all valid frames (no IQR filtering)
    for angle = all_angles(:)'
        % Find indices for this angle in each config
        idx_8port = [];
        if ~isempty(results_8port)
            idx_8port = find([results_8port.angle] == angle, 1);
        end
        idx_4port = [];
        if ~isempty(results_4port)
            idx_4port = find([results_4port.angle] == angle, 1);
        end
        idx_2port = [];
        if ~isempty(results_2port)
            idx_2port = find([results_2port.angle] == angle, 1);
        end
        
        % Use all valid frames for 8-port (no IQR filtering)
        if ~isempty(idx_8port)
            valid_mask = angle_valid_masks_8port{idx_8port};
            angle_filtered_indices_8port{idx_8port} = find(valid_mask);
        end
        
        % Use all valid frames for 4-port (no IQR filtering)
        if ~isempty(idx_4port)
            valid_mask = angle_valid_masks_4port{idx_4port};
            angle_filtered_indices_4port{idx_4port} = find(valid_mask);
        end
        
        % Use all valid frames for 2-port (no IQR filtering)
        if ~isempty(idx_2port)
            valid_mask = angle_valid_masks_2port{idx_2port};
            angle_filtered_indices_2port{idx_2port} = find(valid_mask);
        end
    end
    
    fprintf('  All valid frames collected (no IQR filtering), intersection will be performed\n');
end

% Print frame count summary before filtering
fprintf('\n===== Frame Count Summary (Before Filtering) =====\n');
if ~isempty(results_8port)
    fprintf('8-port configuration:\n');
    for i = 1:length(results_8port)
        angle = results_8port(i).angle;
        fprintf('  Angle %d°: Common frames = %d, Port original frames: ', angle, results_8port(i).Nf);
        for p = 1:length(results_8port(i).ports_to_use)
            fprintf('Port%d=%d ', results_8port(i).ports_to_use(p), results_8port(i).port_original_frames(p));
        end
        fprintf('\n');
    end
end
if ~isempty(results_4port)
    fprintf('4-port configuration:\n');
    for i = 1:length(results_4port)
        angle = results_4port(i).angle;
        fprintf('  Angle %d°: Common frames = %d, Port original frames: ', angle, results_4port(i).Nf);
        for p = 1:length(results_4port(i).ports_to_use)
            fprintf('Port%d=%d ', results_4port(i).ports_to_use(p), results_4port(i).port_original_frames(p));
        end
        fprintf('\n');
    end
end
if ~isempty(results_2port)
    fprintf('2-port configuration:\n');
    for i = 1:length(results_2port)
        angle = results_2port(i).angle;
        fprintf('  Angle %d°: Common frames = %d, Port original frames: ', angle, results_2port(i).Nf);
        for p = 1:length(results_2port(i).ports_to_use)
            fprintf('Port%d=%d ', results_2port(i).ports_to_use(p), results_2port(i).port_original_frames(p));
        end
        fprintf('\n');
    end
end

% Find intersection of filtered frames for each angle (for display only, will be unified later)
if enable_iqr_filter
    fprintf('\n===== Frame Count Summary (After IQR Filtering, Before Unification) =====\n');
else
    fprintf('\n===== Frame Count Summary (After Valid Frame Selection, Before Unification) =====\n');
end


for angle = all_angles(:)'
    % Find indices for this angle in each config
    idx_8port = [];
    if ~isempty(results_8port)
        idx_8port = find([results_8port.angle] == angle, 1);
    end
    idx_4port = [];
    if ~isempty(results_4port)
        idx_4port = find([results_4port.angle] == angle, 1);
    end
    idx_2port = [];
    if ~isempty(results_2port)
        idx_2port = find([results_2port.angle] == angle, 1);
    end
    
    % Get filtered frame indices for each config (within this angle)
    filtered_8port = [];
    filtered_4port = [];
    filtered_2port = [];
    
    if ~isempty(idx_8port) && ~isempty(angle_filtered_indices_8port{idx_8port})
        filtered_8port = angle_filtered_indices_8port{idx_8port};
    end
    if ~isempty(idx_4port) && ~isempty(angle_filtered_indices_4port{idx_4port})
        filtered_4port = angle_filtered_indices_4port{idx_4port};
    end
    if ~isempty(idx_2port) && ~isempty(angle_filtered_indices_2port{idx_2port})
        filtered_2port = angle_filtered_indices_2port{idx_2port};
    end
    
    % Find intersection of filtered frames
    common_frames = [];
    if ~isempty(filtered_8port) && ~isempty(filtered_4port) && ~isempty(filtered_2port)
        common_frames = intersect(intersect(filtered_8port, filtered_4port), filtered_2port);
    elseif ~isempty(filtered_8port) && ~isempty(filtered_4port)
        common_frames = intersect(filtered_8port, filtered_4port);
    elseif ~isempty(filtered_8port) && ~isempty(filtered_2port)
        common_frames = intersect(filtered_8port, filtered_2port);
    elseif ~isempty(filtered_4port) && ~isempty(filtered_2port)
        common_frames = intersect(filtered_4port, filtered_2port);
    elseif ~isempty(filtered_8port)
        common_frames = filtered_8port;
    elseif ~isempty(filtered_4port)
        common_frames = filtered_4port;
    elseif ~isempty(filtered_2port)
        common_frames = filtered_2port;
    end
    
    % Display frame count info (for display only, will be unified later)
    if ~isempty(common_frames)
        if enable_iqr_filter
            % 8-port
            if ~isempty(idx_8port)
                fprintf('  8-port @%d°: Original common=%d, After IQR filtering=%d (removed %d frames)\n', ...
                    results_8port(idx_8port).angle, results_8port(idx_8port).Nf, length(common_frames), results_8port(idx_8port).Nf - length(common_frames));
            end
            
            % 4-port
            if ~isempty(idx_4port)
                fprintf('  4-port @%d°: Original common=%d, After IQR filtering=%d (removed %d frames)\n', ...
                    results_4port(idx_4port).angle, results_4port(idx_4port).Nf, length(common_frames), results_4port(idx_4port).Nf - length(common_frames));
            end
            
            % 2-port
            if ~isempty(idx_2port)
                fprintf('  2-port @%d°: Original common=%d, After IQR filtering=%d (removed %d frames)\n', ...
                    results_2port(idx_2port).angle, results_2port(idx_2port).Nf, length(common_frames), results_2port(idx_2port).Nf - length(common_frames));
            end
        else
            % 8-port
            if ~isempty(idx_8port)
                fprintf('  8-port @%d°: Original common=%d, After intersection=%d (all valid frames used)\n', ...
                    results_8port(idx_8port).angle, results_8port(idx_8port).Nf, length(common_frames));
            end
            
            % 4-port
            if ~isempty(idx_4port)
                fprintf('  4-port @%d°: Original common=%d, After intersection=%d (all valid frames used)\n', ...
                    results_4port(idx_4port).angle, results_4port(idx_4port).Nf, length(common_frames));
            end
            
            % 2-port
            if ~isempty(idx_2port)
                fprintf('  2-port @%d°: Original common=%d, After intersection=%d (all valid frames used)\n', ...
                    results_2port(idx_2port).angle, results_2port(idx_2port).Nf, length(common_frames));
            end
        end
    end
end

%% Step: Unify frame count across all angles (keep minimum error frames)
if unify_frame_count
    fprintf('\n===== Unifying Frame Count Across All Angles =====\n');
    fprintf('  Strategy: Find minimum frame count, then keep top-N minimum error frames for each angle\n');
else
    fprintf('\n===== Skipping Frame Count Unification =====\n');
    fprintf('  Using filtered frames directly (no unification)\n');
end

if unify_frame_count
    % Collect per-angle frame counts (after filtering)
    angle_frame_counts = containers.Map('KeyType', 'double', 'ValueType', 'double');

    for angle = all_angles(:)'
        idx_8port = [];
        if ~isempty(results_8port)
            idx_8port = find([results_8port.angle] == angle, 1);
        end
        idx_4port = [];
        if ~isempty(results_4port)
            idx_4port = find([results_4port.angle] == angle, 1);
        end
        idx_2port = [];
        if ~isempty(results_2port)
            idx_2port = find([results_2port.angle] == angle, 1);
        end
        
        % Get common frames count for this angle
        filtered_8port = [];
        filtered_4port = [];
        filtered_2port = [];
        
        if ~isempty(idx_8port) && ~isempty(angle_filtered_indices_8port{idx_8port})
            filtered_8port = angle_filtered_indices_8port{idx_8port};
        end
        if ~isempty(idx_4port) && ~isempty(angle_filtered_indices_4port{idx_4port})
            filtered_4port = angle_filtered_indices_4port{idx_4port};
        end
        if ~isempty(idx_2port) && ~isempty(angle_filtered_indices_2port{idx_2port})
            filtered_2port = angle_filtered_indices_2port{idx_2port};
        end
        
        % Find intersection
        common_frames = [];
        if ~isempty(filtered_8port) && ~isempty(filtered_4port) && ~isempty(filtered_2port)
            common_frames = intersect(intersect(filtered_8port, filtered_4port), filtered_2port);
        elseif ~isempty(filtered_8port) && ~isempty(filtered_4port)
            common_frames = intersect(filtered_8port, filtered_4port);
        elseif ~isempty(filtered_8port) && ~isempty(filtered_2port)
            common_frames = intersect(filtered_8port, filtered_2port);
        elseif ~isempty(filtered_4port) && ~isempty(filtered_2port)
            common_frames = intersect(filtered_4port, filtered_2port);
        elseif ~isempty(filtered_8port)
            common_frames = filtered_8port;
        elseif ~isempty(filtered_4port)
            common_frames = filtered_4port;
        elseif ~isempty(filtered_2port)
            common_frames = filtered_2port;
        end
        
        angle_frame_counts(angle) = length(common_frames);
    end

    % Find minimum frame count
    min_frame_count = inf;
    for angle = all_angles(:)'
        if isKey(angle_frame_counts, angle)
            min_frame_count = min(min_frame_count, angle_frame_counts(angle));
        end
    end

    fprintf('  Minimum frame count across all angles: %d frames\n', min_frame_count);

    % For each angle, keep top-N minimum error frames
    angle_unified_frames = containers.Map('KeyType', 'double', 'ValueType', 'any');

    for angle = all_angles(:)'
        if ~isKey(angle_frame_counts, angle)
            continue;
        end
        
        current_count = angle_frame_counts(angle);
        if current_count <= min_frame_count
            % Already at or below minimum, use all frames
            fprintf('  Angle %d°: %d frames (no reduction needed)\n', angle, current_count);
            % Store original common frames
            idx_8port = [];
            if ~isempty(results_8port)
                idx_8port = find([results_8port.angle] == angle, 1);
            end
            idx_4port = [];
            if ~isempty(results_4port)
                idx_4port = find([results_4port.angle] == angle, 1);
            end
            idx_2port = [];
            if ~isempty(results_2port)
                idx_2port = find([results_2port.angle] == angle, 1);
            end
            
            filtered_8port = [];
            filtered_4port = [];
            filtered_2port = [];
            
            if ~isempty(idx_8port) && ~isempty(angle_filtered_indices_8port{idx_8port})
                filtered_8port = angle_filtered_indices_8port{idx_8port};
            end
            if ~isempty(idx_4port) && ~isempty(angle_filtered_indices_4port{idx_4port})
                filtered_4port = angle_filtered_indices_4port{idx_4port};
            end
            if ~isempty(idx_2port) && ~isempty(angle_filtered_indices_2port{idx_2port})
                filtered_2port = angle_filtered_indices_2port{idx_2port};
            end
            
            common_frames = [];
            if ~isempty(filtered_8port) && ~isempty(filtered_4port) && ~isempty(filtered_2port)
                common_frames = intersect(intersect(filtered_8port, filtered_4port), filtered_2port);
            elseif ~isempty(filtered_8port) && ~isempty(filtered_4port)
                common_frames = intersect(filtered_8port, filtered_4port);
            elseif ~isempty(filtered_8port) && ~isempty(filtered_2port)
                common_frames = intersect(filtered_8port, filtered_2port);
            elseif ~isempty(filtered_4port) && ~isempty(filtered_2port)
                common_frames = intersect(filtered_4port, filtered_2port);
            elseif ~isempty(filtered_8port)
                common_frames = filtered_8port;
            elseif ~isempty(filtered_4port)
                common_frames = filtered_4port;
            elseif ~isempty(filtered_2port)
                common_frames = filtered_2port;
            end
            
            angle_unified_frames(angle) = struct('frames', common_frames, 'idx_8port', idx_8port, 'idx_4port', idx_4port, 'idx_2port', idx_2port);
        else
            % Need to reduce: sort by error and keep top-N minimum error frames
            fprintf('  Angle %d°: %d frames -> %d frames (keeping minimum error frames)\n', angle, current_count, min_frame_count);
            
            % Get frame errors for this angle (use 8-port if available, otherwise 4-port, otherwise 2-port)
            idx_8port = [];
            if ~isempty(results_8port)
                idx_8port = find([results_8port.angle] == angle, 1);
            end
            idx_4port = [];
            if ~isempty(results_4port)
                idx_4port = find([results_4port.angle] == angle, 1);
            end
            idx_2port = [];
            if ~isempty(results_2port)
                idx_2port = find([results_2port.angle] == angle, 1);
            end
            
            % Get common frames and errors
            filtered_8port = [];
            filtered_4port = [];
            filtered_2port = [];
            
            if ~isempty(idx_8port) && ~isempty(angle_filtered_indices_8port{idx_8port})
                filtered_8port = angle_filtered_indices_8port{idx_8port};
            end
            if ~isempty(idx_4port) && ~isempty(angle_filtered_indices_4port{idx_4port})
                filtered_4port = angle_filtered_indices_4port{idx_4port};
            end
            if ~isempty(idx_2port) && ~isempty(angle_filtered_indices_2port{idx_2port})
                filtered_2port = angle_filtered_indices_2port{idx_2port};
            end
            
            common_frames = [];
            if ~isempty(filtered_8port) && ~isempty(filtered_4port) && ~isempty(filtered_2port)
                common_frames = intersect(intersect(filtered_8port, filtered_4port), filtered_2port);
            elseif ~isempty(filtered_8port) && ~isempty(filtered_4port)
                common_frames = intersect(filtered_8port, filtered_4port);
            elseif ~isempty(filtered_8port) && ~isempty(filtered_2port)
                common_frames = intersect(filtered_8port, filtered_2port);
            elseif ~isempty(filtered_4port) && ~isempty(filtered_2port)
                common_frames = intersect(filtered_4port, filtered_2port);
            elseif ~isempty(filtered_8port)
                common_frames = filtered_8port;
            elseif ~isempty(filtered_4port)
                common_frames = filtered_4port;
            elseif ~isempty(filtered_2port)
                common_frames = filtered_2port;
            end
            
            % Get errors for sorting (prefer 8-port, then 4-port, then 2-port)
            frame_errors = [];
            if ~isempty(idx_8port)
                aoa_cal = results_8port(idx_8port).aoa_cal;
                valid_mask = angle_valid_masks_8port{idx_8port};
                aoa_cal_valid = aoa_cal(valid_mask);
                true_angle = results_8port(idx_8port).angle;
                frame_errors = abs(aoa_cal_valid - true_angle);
            elseif ~isempty(idx_4port)
                aoa_cal = results_4port(idx_4port).aoa_cal;
                valid_mask = angle_valid_masks_4port{idx_4port};
                aoa_cal_valid = aoa_cal(valid_mask);
                true_angle = results_4port(idx_4port).angle;
                frame_errors = abs(aoa_cal_valid - true_angle);
            elseif ~isempty(idx_2port)
                aoa_cal = results_2port(idx_2port).aoa_cal;
                valid_mask = angle_valid_masks_2port{idx_2port};
                aoa_cal_valid = aoa_cal(valid_mask);
                true_angle = results_2port(idx_2port).angle;
                frame_errors = abs(aoa_cal_valid - true_angle);
            end
            
            if length(frame_errors) >= length(common_frames)
                % Sort by error and keep top-N minimum error frames
                [~, sort_idx] = sort(frame_errors(common_frames), 'ascend');
                selected_frames = common_frames(sort_idx(1:min_frame_count));
                angle_unified_frames(angle) = struct('frames', selected_frames, 'idx_8port', idx_8port, 'idx_4port', idx_4port, 'idx_2port', idx_2port);
            else
                % Fallback: use all common frames
                angle_unified_frames(angle) = struct('frames', common_frames, 'idx_8port', idx_8port, 'idx_4port', idx_4port, 'idx_2port', idx_2port);
            end
        end
    end

    fprintf('  Frame count unified: all angles now use %d frames\n', min_frame_count);
else
    % Skip unification: directly use filtered frames (intersection of filtered frames for each angle)
    fprintf('  Building frame map from filtered frames (no unification)...\n');
    angle_unified_frames = containers.Map('KeyType', 'double', 'ValueType', 'any');
    
    for angle = all_angles(:)'
        % Find indices for this angle in each config
        idx_8port = [];
        if ~isempty(results_8port)
            idx_8port = find([results_8port.angle] == angle, 1);
        end
        idx_4port = [];
        if ~isempty(results_4port)
            idx_4port = find([results_4port.angle] == angle, 1);
        end
        idx_2port = [];
        if ~isempty(results_2port)
            idx_2port = find([results_2port.angle] == angle, 1);
        end
        
        % Get filtered frame indices for each config (within this angle)
        filtered_8port = [];
        filtered_4port = [];
        filtered_2port = [];
        
        if ~isempty(idx_8port) && ~isempty(angle_filtered_indices_8port{idx_8port})
            filtered_8port = angle_filtered_indices_8port{idx_8port};
        end
        if ~isempty(idx_4port) && ~isempty(angle_filtered_indices_4port{idx_4port})
            filtered_4port = angle_filtered_indices_4port{idx_4port};
        end
        if ~isempty(idx_2port) && ~isempty(angle_filtered_indices_2port{idx_2port})
            filtered_2port = angle_filtered_indices_2port{idx_2port};
        end
        
        % Find intersection of filtered frames
        common_frames = [];
        if ~isempty(filtered_8port) && ~isempty(filtered_4port) && ~isempty(filtered_2port)
            common_frames = intersect(intersect(filtered_8port, filtered_4port), filtered_2port);
        elseif ~isempty(filtered_8port) && ~isempty(filtered_4port)
            common_frames = intersect(filtered_8port, filtered_4port);
        elseif ~isempty(filtered_8port) && ~isempty(filtered_2port)
            common_frames = intersect(filtered_8port, filtered_2port);
        elseif ~isempty(filtered_4port) && ~isempty(filtered_2port)
            common_frames = intersect(filtered_4port, filtered_2port);
        elseif ~isempty(filtered_8port)
            common_frames = filtered_8port;
        elseif ~isempty(filtered_4port)
            common_frames = filtered_4port;
        elseif ~isempty(filtered_2port)
            common_frames = filtered_2port;
        end
        
        % Store common frames without unification
        if ~isempty(common_frames)
            angle_unified_frames(angle) = struct('frames', common_frames, 'idx_8port', idx_8port, 'idx_4port', idx_4port, 'idx_2port', idx_2port);
            fprintf('  Angle %d°: %d frames (using all filtered frames, no unification)\n', angle, length(common_frames));
        end
    end
    
    fprintf('  Frame count not unified: each angle uses its own filtered frame count\n');
end

% Re-collect frame errors using unified frames
all_frame_errors_8port_filtered = [];
all_frame_errors_4port_filtered = [];
all_frame_errors_2port_filtered = [];

if unify_frame_count
    fprintf('\n  Re-collecting frame errors using unified frames:\n');
else
    fprintf('\n  Re-collecting frame errors using filtered frames:\n');
end
for angle = all_angles(:)'
    if ~isKey(angle_unified_frames, angle)
        continue;
    end
    
    info = angle_unified_frames(angle);
    unified_frames = info.frames;
    
    if ~isempty(unified_frames)
        % 8-port
        if ~isempty(info.idx_8port)
            aoa_cal_8port = results_8port(info.idx_8port).aoa_cal;
            valid_mask_8port = angle_valid_masks_8port{info.idx_8port};
            aoa_cal_8port_valid = aoa_cal_8port(valid_mask_8port);
            if length(aoa_cal_8port_valid) >= max(unified_frames)
                true_angle = results_8port(info.idx_8port).angle;
                angle_errors_8port = abs(aoa_cal_8port_valid - true_angle);
                all_frame_errors_8port_filtered = [all_frame_errors_8port_filtered; angle_errors_8port(unified_frames)]; %#ok<AGROW>
            end
        end
        
        % 4-port
        if ~isempty(info.idx_4port)
            aoa_cal_4port = results_4port(info.idx_4port).aoa_cal;
            valid_mask_4port = angle_valid_masks_4port{info.idx_4port};
            aoa_cal_4port_valid = aoa_cal_4port(valid_mask_4port);
            if length(aoa_cal_4port_valid) >= max(unified_frames)
                true_angle = results_4port(info.idx_4port).angle;
                angle_errors_4port = abs(aoa_cal_4port_valid - true_angle);
                all_frame_errors_4port_filtered = [all_frame_errors_4port_filtered; angle_errors_4port(unified_frames)]; %#ok<AGROW>
            end
        end
        
        % 2-port
        if ~isempty(info.idx_2port)
            aoa_cal_2port = results_2port(info.idx_2port).aoa_cal;
            valid_mask_2port = angle_valid_masks_2port{info.idx_2port};
            aoa_cal_2port_valid = aoa_cal_2port(valid_mask_2port);
            if length(aoa_cal_2port_valid) >= max(unified_frames)
                true_angle = results_2port(info.idx_2port).angle;
                angle_errors_2port = abs(aoa_cal_2port_valid - true_angle);
                all_frame_errors_2port_filtered = [all_frame_errors_2port_filtered; angle_errors_2port(unified_frames)]; %#ok<AGROW>
            end
        end
    end
end

if unify_frame_count
    fprintf('    Unified frame errors collected:\n');
else
    fprintf('    Filtered frame errors collected:\n');
end
fprintf('      8-port: N=%d frames\n', length(all_frame_errors_8port_filtered));
fprintf('      4-port: N=%d frames\n', length(all_frame_errors_4port_filtered));
fprintf('      2-port: N=%d frames\n', length(all_frame_errors_2port_filtered));

%% Compute per-angle filtered statistics (using unified/filtered frames)
if unify_frame_count
    fprintf('\nComputing per-angle filtered statistics (using unified frames)...\n');
    % Use unified frames instead of recomputing
    % angle_unified_frames already contains the unified frames for each angle
else
    fprintf('\nComputing per-angle filtered statistics (using filtered frames)...\n');
    % Use filtered frames (no unification)
    % angle_unified_frames contains the filtered frames for each angle
end

% 8-port filtered stats (using unified frames)
error_8port_filtered = nan(length(results_8port), 1);
std_8port_filtered = nan(length(results_8port), 1);
std_aoa_8port_filtered = nan(length(results_8port), 1);
mean_aoa_8port_filtered = nan(length(results_8port), 1);  % Filtered mean AoA for Subplot 1
if ~isempty(results_8port)
    for i = 1:length(results_8port)
        angle = results_8port(i).angle;
        if isKey(angle_unified_frames, angle)
            info = angle_unified_frames(angle);
            common_frames = info.frames;
            if ~isempty(common_frames) && ~isempty(info.idx_8port)
                aoa_cal_angle = results_8port(i).aoa_cal;
                valid_mask = angle_valid_masks_8port{i};
                aoa_cal_valid = aoa_cal_angle(valid_mask);
                if length(aoa_cal_valid) >= max(common_frames)
                    true_angle = results_8port(i).angle;
                    angle_errors = abs(aoa_cal_valid - true_angle);
                    angle_errors_common = angle_errors(common_frames);
                    if ~isempty(angle_errors_common)
                        error_8port_filtered(i) = mean(angle_errors_common);
                        std_8port_filtered(i) = std(angle_errors_common);
                    end
                    % Also compute filtered mean and std of AoA estimates (for Subplot 1 and 3)
                    aoa_cal_common = aoa_cal_valid(common_frames);
                    if ~isempty(aoa_cal_common)
                        mean_aoa_8port_filtered(i) = mean(aoa_cal_common);
                        std_aoa_8port_filtered(i) = std(aoa_cal_common);
                    end
                end
            end
        end
    end
end

% 4-port filtered stats (using unified frames)
error_4port_filtered = nan(length(results_4port), 1);
std_4port_filtered = nan(length(results_4port), 1);
std_aoa_4port_filtered = nan(length(results_4port), 1);
mean_aoa_4port_filtered = nan(length(results_4port), 1);  % Filtered mean AoA for Subplot 1
if ~isempty(results_4port)
    for i = 1:length(results_4port)
        angle = results_4port(i).angle;
        if isKey(angle_unified_frames, angle)
            info = angle_unified_frames(angle);
            common_frames = info.frames;
            if ~isempty(common_frames) && ~isempty(info.idx_4port)
                aoa_cal_angle = results_4port(i).aoa_cal;
                valid_mask = angle_valid_masks_4port{i};
                aoa_cal_valid = aoa_cal_angle(valid_mask);
                if length(aoa_cal_valid) >= max(common_frames)
                    true_angle = results_4port(i).angle;
                    angle_errors = abs(aoa_cal_valid - true_angle);
                    angle_errors_common = angle_errors(common_frames);
                    if ~isempty(angle_errors_common)
                        error_4port_filtered(i) = mean(angle_errors_common);
                        std_4port_filtered(i) = std(angle_errors_common);
                    end
                    % Also compute filtered mean and std of AoA estimates (for Subplot 1 and 3)
                    aoa_cal_common = aoa_cal_valid(common_frames);
                    if ~isempty(aoa_cal_common)
                        mean_aoa_4port_filtered(i) = mean(aoa_cal_common);
                        std_aoa_4port_filtered(i) = std(aoa_cal_common);
                    end
                end
            end
        end
    end
end

% 2-port filtered stats (using unified frames)
error_2port_filtered = nan(length(results_2port), 1);
std_2port_filtered = nan(length(results_2port), 1);
std_aoa_2port_filtered = nan(length(results_2port), 1);
mean_aoa_2port_filtered = nan(length(results_2port), 1);  % Filtered mean AoA for Subplot 1
if ~isempty(results_2port)
    for i = 1:length(results_2port)
        angle = results_2port(i).angle;
        if isKey(angle_unified_frames, angle)
            info = angle_unified_frames(angle);
            common_frames = info.frames;
            if ~isempty(common_frames) && ~isempty(info.idx_2port)
                aoa_cal_angle = results_2port(i).aoa_cal;
                valid_mask = angle_valid_masks_2port{i};
                aoa_cal_valid = aoa_cal_angle(valid_mask);
                if length(aoa_cal_valid) >= max(common_frames)
                    true_angle = results_2port(i).angle;
                    angle_errors = abs(aoa_cal_valid - true_angle);
                    angle_errors_common = angle_errors(common_frames);
                    if ~isempty(angle_errors_common)
                        error_2port_filtered(i) = mean(angle_errors_common);
                        std_2port_filtered(i) = std(angle_errors_common);
                    end
                    % Also compute filtered mean and std of AoA estimates (for Subplot 1 and 3)
                    aoa_cal_common = aoa_cal_valid(common_frames);
                    if ~isempty(aoa_cal_common)
                        mean_aoa_2port_filtered(i) = mean(aoa_cal_common);
                        std_aoa_2port_filtered(i) = std(aoa_cal_common);
                    end
                end
            end
        end
    end
end

%% Summary
fprintf('\n========================================\n');
fprintf('   RESULTS SUMMARY\n');
fprintf('========================================\n');

if ~isempty(results_8port)
    fprintf('\n8-port (calibrated):\n');
    fprintf('  Mean error: %.2f° ± %.2f°\n', mean([results_8port.error_cal]), std([results_8port.error_cal]));
    fprintf('  Per-frame (all): %.2f° ± %.2f° (N=%d)\n', mean(all_frame_errors_8port), std(all_frame_errors_8port), length(all_frame_errors_8port));
    if ~isempty(all_frame_errors_8port_filtered)
        fprintf('  Per-frame (intersection): %.2f° ± %.2f° (N=%d)\n', ...
            mean(all_frame_errors_8port_filtered), std(all_frame_errors_8port_filtered), ...
            length(all_frame_errors_8port_filtered));
    end
end

if ~isempty(results_4port)
    fprintf('\n4-port (calibrated):\n');
    fprintf('  Mean error: %.2f° ± %.2f°\n', mean([results_4port.error_cal]), std([results_4port.error_cal]));
    fprintf('  Per-frame (all): %.2f° ± %.2f° (N=%d)\n', mean(all_frame_errors_4port), std(all_frame_errors_4port), length(all_frame_errors_4port));
    if ~isempty(all_frame_errors_4port_filtered)
        fprintf('  Per-frame (intersection): %.2f° ± %.2f° (N=%d)\n', ...
            mean(all_frame_errors_4port_filtered), std(all_frame_errors_4port_filtered), ...
            length(all_frame_errors_4port_filtered));
    end
end

if ~isempty(results_2port)
    fprintf('\n2-port (calibrated):\n');
    fprintf('  Mean error: %.2f° ± %.2f°\n', mean([results_2port.error_cal]), std([results_2port.error_cal]));
    fprintf('  Per-frame (all): %.2f° ± %.2f° (N=%d)\n', mean(all_frame_errors_2port), std(all_frame_errors_2port), length(all_frame_errors_2port));
    if ~isempty(all_frame_errors_2port_filtered)
        fprintf('  Per-frame (intersection): %.2f° ± %.2f° (N=%d)\n', ...
            mean(all_frame_errors_2port_filtered), std(all_frame_errors_2port_filtered), ...
            length(all_frame_errors_2port_filtered));
    end
end

if ~isempty(results_uncal)
    fprintf('\nUncalibrated (8-port):\n');
    fprintf('  Mean error: %.2f° ± %.2f°\n', mean([results_uncal.error_uncal]), std([results_uncal.error_uncal]));
    fprintf('  Per-frame: %.2f° ± %.2f° (N=%d)\n', mean(all_frame_errors_uncal), std(all_frame_errors_uncal), length(all_frame_errors_uncal));
end

%% Plot
fprintf('\n===== Generating plot =====\n');

figure('Color','w','Position',[50 50 1800 900]);

% Get angles for each configuration
angles_8port = [];
angles_4port = [];
angles_2port = [];
angles_uncal = [];

if ~isempty(results_8port)
    angles_8port = [results_8port.angle]';
end
if ~isempty(results_4port)
    angles_4port = [results_4port.angle]';
end
if ~isempty(results_2port)
    angles_2port = [results_2port.angle]';
end
if ~isempty(results_uncal)
    angles_uncal = [results_uncal.angle]';
end

% Use the first available angles for common x-axis
if ~isempty(angles_8port)
    angles = angles_8port;
elseif ~isempty(angles_4port)
    angles = angles_4port;
elseif ~isempty(angles_2port)
    angles = angles_2port;
elseif ~isempty(angles_uncal)
    angles = angles_uncal;
else
    error('No results available for plotting');
end

% Subplot 1: Estimated vs True Angle (using filtered data)
subplot(2,4,1);
hold on;
plot([-30 30], [-30 30], 'k--', 'LineWidth', 2, 'DisplayName', 'Ideal');

if ~isempty(results_8port)
    % Use filtered mean AoA and std (using intersection)
    errorbar(angles_8port, mean_aoa_8port_filtered, std_aoa_8port_filtered, ...
        's-', 'LineWidth', 2.5, 'MarkerSize', 10, ...
        'Color', [0.2 0.8 0.2], 'MarkerFaceColor', [0.2 0.8 0.2], 'DisplayName', '8-port (cal, filtered)');
end

if ~isempty(results_4port)
    % Use filtered mean AoA and std (using intersection)
    errorbar(angles_4port, mean_aoa_4port_filtered, std_aoa_4port_filtered, ...
        '^-', 'LineWidth', 2, 'MarkerSize', 9, ...
        'Color', [0 0.6 1], 'MarkerFaceColor', [0 0.6 1], 'DisplayName', '4-port (cal, filtered)');
end

if ~isempty(results_2port)
    % Use filtered mean AoA and std (using intersection)
    errorbar(angles_2port, mean_aoa_2port_filtered, std_aoa_2port_filtered, ...
        'd-', 'LineWidth', 2, 'MarkerSize', 8, ...
        'Color', [1 0.6 0], 'MarkerFaceColor', [1 0.6 0], 'DisplayName', '2-port (cal, filtered)');
end

if ~isempty(results_uncal)
    % Uncalibrated uses all frames (no filtering)
    errorbar(angles_uncal, [results_uncal.aoa_uncal_mean]', [results_uncal.aoa_uncal_std]', ...
        'o-', 'LineWidth', 2, 'MarkerSize', 8, ...
        'Color', [0.8 0.2 0.2], 'MarkerFaceColor', [0.8 0.2 0.2], 'DisplayName', 'Uncalibrated');
end

grid on;
xlabel('True Angle (°)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Estimated Angle (°)', 'FontSize', 12, 'FontWeight', 'bold');
title('Estimated vs True Angle', 'FontSize', 13, 'FontWeight', 'bold');
legend('Location', 'northwest', 'FontSize', 9);
axis equal;
xlim([min(angles)-5, max(angles)+5]);
ylim([min(angles)-5, max(angles)+5]);

% Subplot 2: Error comparison (using filtered per-angle statistics)
subplot(2,4,2);
hold on;

if ~isempty(results_8port)
    % Use filtered per-angle error and std (using intersection)
    errorbar(angles_8port, error_8port_filtered, std_8port_filtered, ...
        's-', 'LineWidth', 2.5, 'MarkerSize', 10, ...
        'Color', [0.2 0.8 0.2], 'MarkerFaceColor', [0.2 0.8 0.2], 'DisplayName', '8-port (intersection)');
    if ~isempty(all_frame_errors_8port_filtered)
        yline(mean(all_frame_errors_8port_filtered), '--', 'Color', [0.2 0.8 0.2], 'LineWidth', 1.5, ...
            'Label', sprintf('8-port Mean (%.1f°)', mean(all_frame_errors_8port_filtered)));
    end
end

if ~isempty(results_4port)
    % Use filtered per-angle error and std (using intersection)
    errorbar(angles_4port, error_4port_filtered, std_4port_filtered, ...
        '^-', 'LineWidth', 2, 'MarkerSize', 9, ...
        'Color', [0 0.6 1], 'MarkerFaceColor', [0 0.6 1], 'DisplayName', '4-port (intersection)');
    if ~isempty(all_frame_errors_4port_filtered)
        yline(mean(all_frame_errors_4port_filtered), '--', 'Color', [0 0.6 1], 'LineWidth', 1.5, ...
            'Label', sprintf('4-port Mean (%.1f°)', mean(all_frame_errors_4port_filtered)));
    end
end

if ~isempty(results_2port)
    % Use filtered per-angle error and std (using intersection)
    errorbar(angles_2port, error_2port_filtered, std_2port_filtered, ...
        'd-', 'LineWidth', 2, 'MarkerSize', 8, ...
        'Color', [1 0.6 0], 'MarkerFaceColor', [1 0.6 0], 'DisplayName', '2-port (intersection)');
    if ~isempty(all_frame_errors_2port_filtered)
        yline(mean(all_frame_errors_2port_filtered), '--', 'Color', [1 0.6 0], 'LineWidth', 1.5, ...
            'Label', sprintf('2-port Mean (%.1f°)', mean(all_frame_errors_2port_filtered)));
    end
end

if ~isempty(results_uncal)
    errorbar(angles_uncal, [results_uncal.error_uncal]', [results_uncal.aoa_uncal_std]', ...
        'o-', 'LineWidth', 2, 'MarkerSize', 8, ...
        'Color', [0.8 0.2 0.2], 'MarkerFaceColor', [0.8 0.2 0.2], 'DisplayName', 'Uncalibrated');
    yline(mean(all_frame_errors_uncal), '--', 'Color', 'r', 'LineWidth', 1.5, ...
        'Label', sprintf('Uncal Mean (%.1f°)', mean(all_frame_errors_uncal)));
end

grid on;
xlabel('True Angle (°)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Absolute Error (°)', 'FontSize', 12, 'FontWeight', 'bold');
title('AoA Estimation Error', 'FontSize', 13, 'FontWeight', 'bold');
legend('Location', 'best', 'FontSize', 9);

% Subplot 3: Standard deviation (using filtered per-angle statistics)
subplot(2,4,3);
hold on;

if ~isempty(results_8port)
    plot(angles_8port, std_aoa_8port_filtered, 's-', 'LineWidth', 2.5, 'MarkerSize', 10, ...
        'Color', [0.2 0.8 0.2], 'MarkerFaceColor', [0.2 0.8 0.2], 'DisplayName', '8-port (intersection)');
end

if ~isempty(results_4port)
    plot(angles_4port, std_aoa_4port_filtered, '^-', 'LineWidth', 2, 'MarkerSize', 9, ...
        'Color', [0 0.6 1], 'MarkerFaceColor', [0 0.6 1], 'DisplayName', '4-port (intersection)');
end

if ~isempty(results_2port)
    plot(angles_2port, std_aoa_2port_filtered, 'd-', 'LineWidth', 2, 'MarkerSize', 8, ...
        'Color', [1 0.6 0], 'MarkerFaceColor', [1 0.6 0], 'DisplayName', '2-port (intersection)');
end

if ~isempty(results_uncal)
    plot(angles_uncal, [results_uncal.aoa_uncal_std]', 'o-', 'LineWidth', 2, 'MarkerSize', 8, ...
        'Color', [0.8 0.2 0.2], 'MarkerFaceColor', [0.8 0.2 0.2], 'DisplayName', 'Uncalibrated');
end

grid on;
xlabel('True Angle (°)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Standard Deviation (°)', 'FontSize', 12, 'FontWeight', 'bold');
title('Estimation Consistency', 'FontSize', 13, 'FontWeight', 'bold');
legend('Location', 'best', 'FontSize', 9);

% Subplot 4: CDF (using filtered data)
subplot(2,4,4);
hold on;

if ~isempty(all_frame_errors_8port_filtered)
    sorted_8port = sort(all_frame_errors_8port_filtered);
    cdf_8port = (1:length(sorted_8port)) / length(sorted_8port);
    plot(sorted_8port, cdf_8port * 100, '-', 'LineWidth', 2.5, ...
        'Color', [0.2 0.8 0.2], 'DisplayName', sprintf('8-port (N=%d, intersection)', length(all_frame_errors_8port_filtered)));
end

if ~isempty(all_frame_errors_4port_filtered)
    sorted_4port = sort(all_frame_errors_4port_filtered);
    cdf_4port = (1:length(sorted_4port)) / length(sorted_4port);
    plot(sorted_4port, cdf_4port * 100, '-', 'LineWidth', 2.5, ...
        'Color', [0 0.6 1], 'DisplayName', sprintf('4-port (N=%d, intersection)', length(all_frame_errors_4port_filtered)));
end

if ~isempty(all_frame_errors_2port_filtered)
    sorted_2port = sort(all_frame_errors_2port_filtered);
    cdf_2port = (1:length(sorted_2port)) / length(sorted_2port);
    plot(sorted_2port, cdf_2port * 100, '-', 'LineWidth', 2.5, ...
        'Color', [1 0.6 0], 'DisplayName', sprintf('2-port (N=%d, intersection)', length(all_frame_errors_2port_filtered)));
end

if ~isempty(all_frame_errors_uncal)
    sorted_uncal = sort(all_frame_errors_uncal);
    cdf_uncal = (1:length(sorted_uncal)) / length(sorted_uncal);
    plot(sorted_uncal, cdf_uncal * 100, '-', 'LineWidth', 2.5, ...
        'Color', [0.8 0.2 0.2], 'DisplayName', sprintf('Uncal (N=%d)', length(all_frame_errors_uncal)));
end

yline(50, ':', 'Color', [0.5 0.5 0.5], 'LineWidth', 0.8, 'Label', '50%');
yline(90, ':', 'Color', [0.5 0.5 0.5], 'LineWidth', 0.8, 'Label', '90%');

grid on;
xlabel('Absolute Error (°)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Cumulative Probability (%)', 'FontSize', 12, 'FontWeight', 'bold');
title('Error CDF', 'FontSize', 13, 'FontWeight', 'bold');
legend('Location', 'southeast', 'FontSize', 9);
ylim([0 105]);

% Subplot 5: Improvement comparison (using filtered data)
subplot(2,4,5);
hold on;
if ~isempty(results_8port) && ~isempty(results_uncal)
    % Use filtered error for 8-port
    improvement_8port = [results_uncal.error_uncal]' - error_8port_filtered;
    bar(angles_8port, improvement_8port, 'FaceColor', [0.2 0.6 0.8], 'EdgeColor', 'none', 'FaceAlpha', 0.7);
    yline(0, '--', 'Color', 'k', 'LineWidth', 1.5);
    grid on;
    xlabel('True Angle (°)', 'FontSize', 12, 'FontWeight', 'bold');
    ylabel('Error Reduction (°)', 'FontSize', 12, 'FontWeight', 'bold');
    title('8-port Calibration Improvement (filtered)', 'FontSize', 13, 'FontWeight', 'bold');
end

% Subplot 6: Box plot comparison (using filtered data)
subplot(2,4,6);
hold on;
box_data = [];
box_groups = {};
if ~isempty(all_frame_errors_8port_filtered)
    box_data = [box_data; all_frame_errors_8port_filtered(:)];
    box_groups = [box_groups; repmat({'8-port'}, length(all_frame_errors_8port_filtered), 1)];
end
if ~isempty(all_frame_errors_4port_filtered)
    box_data = [box_data; all_frame_errors_4port_filtered(:)];
    box_groups = [box_groups; repmat({'4-port'}, length(all_frame_errors_4port_filtered), 1)];
end
if ~isempty(all_frame_errors_2port_filtered)
    box_data = [box_data; all_frame_errors_2port_filtered(:)];
    box_groups = [box_groups; repmat({'2-port'}, length(all_frame_errors_2port_filtered), 1)];
end
if ~isempty(all_frame_errors_uncal)
    box_data = [box_data; all_frame_errors_uncal(:)];
    box_groups = [box_groups; repmat({'Uncal'}, length(all_frame_errors_uncal), 1)];
end

if ~isempty(box_data)
    boxplot(box_data, box_groups, 'Colors', [0.2 0.8 0.2; 0 0.6 1; 1 0.6 0; 0.8 0.2 0.2], 'Widths', 0.5);
    grid on;
    ylabel('Absolute Error (°)', 'FontSize', 12, 'FontWeight', 'bold');
    title('Error Distribution', 'FontSize', 13, 'FontWeight', 'bold');
end

% Subplot 7: Histogram (using filtered data)
subplot(2,4,7);
hold on;
if ~isempty(all_frame_errors_8port_filtered)
    histogram(all_frame_errors_8port_filtered, 'BinWidth', 1, 'FaceColor', [0.2 0.8 0.2], ...
        'FaceAlpha', 0.7, 'EdgeColor', 'none', 'DisplayName', sprintf('8-port (N=%d, intersection)', length(all_frame_errors_8port_filtered)));
end
if ~isempty(all_frame_errors_4port_filtered)
    histogram(all_frame_errors_4port_filtered, 'BinWidth', 1, 'FaceColor', [0 0.6 1], ...
        'FaceAlpha', 0.7, 'EdgeColor', 'none', 'DisplayName', sprintf('4-port (N=%d, intersection)', length(all_frame_errors_4port_filtered)));
end
if ~isempty(all_frame_errors_2port_filtered)
    histogram(all_frame_errors_2port_filtered, 'BinWidth', 1, 'FaceColor', [1 0.6 0], ...
        'FaceAlpha', 0.7, 'EdgeColor', 'none', 'DisplayName', sprintf('2-port (N=%d, intersection)', length(all_frame_errors_2port_filtered)));
end
if ~isempty(all_frame_errors_uncal)
    histogram(all_frame_errors_uncal, 'BinWidth', 1, 'FaceColor', [0.8 0.2 0.2], ...
        'FaceAlpha', 0.7, 'EdgeColor', 'none', 'DisplayName', sprintf('Uncal (N=%d)', length(all_frame_errors_uncal)));
end
grid on;
xlabel('Absolute Error (°)', 'FontSize', 12, 'FontWeight', 'bold');
ylabel('Count', 'FontSize', 12, 'FontWeight', 'bold');
title('Error Histogram', 'FontSize', 13, 'FontWeight', 'bold');
legend('Location', 'best', 'FontSize', 9);

% Subplot 8: Summary text
subplot(2,4,8);
axis off;
text(0.05, 0.95, 'SUMMARY:', 'FontSize', 14, 'FontWeight', 'bold');
text(0.05, 0.88, sprintf('Test angles: %d to %d°', min(angles), max(angles)), 'FontSize', 9);

y_pos = 0.80;
if ~isempty(results_8port) && ~isempty(all_frame_errors_8port_filtered)
    text(0.05, y_pos, '8-port (intersection):', 'FontSize', 10, 'FontWeight', 'bold', 'Color', [0.2 0.8 0.2]);
    text(0.1, y_pos-0.05, sprintf('Mean: %.2f° ± %.2f°', mean(all_frame_errors_8port_filtered), std(all_frame_errors_8port_filtered)), 'FontSize', 8);
    text(0.1, y_pos-0.10, sprintf('Median: %.2f°', median(all_frame_errors_8port_filtered)), 'FontSize', 8);
    y_pos = y_pos - 0.20;
end

if ~isempty(results_4port) && ~isempty(all_frame_errors_4port_filtered)
    text(0.05, y_pos, '4-port (intersection):', 'FontSize', 10, 'FontWeight', 'bold', 'Color', [0 0.6 1]);
    text(0.1, y_pos-0.05, sprintf('Mean: %.2f° ± %.2f°', mean(all_frame_errors_4port_filtered), std(all_frame_errors_4port_filtered)), 'FontSize', 8);
    text(0.1, y_pos-0.10, sprintf('Median: %.2f°', median(all_frame_errors_4port_filtered)), 'FontSize', 8);
    y_pos = y_pos - 0.20;
end

if ~isempty(results_2port) && ~isempty(all_frame_errors_2port_filtered)
    text(0.05, y_pos, '2-port (intersection):', 'FontSize', 10, 'FontWeight', 'bold', 'Color', [1 0.6 0]);
    text(0.1, y_pos-0.05, sprintf('Mean: %.2f° ± %.2f°', mean(all_frame_errors_2port_filtered), std(all_frame_errors_2port_filtered)), 'FontSize', 8);
    text(0.1, y_pos-0.10, sprintf('Median: %.2f°', median(all_frame_errors_2port_filtered)), 'FontSize', 8);
    y_pos = y_pos - 0.20;
end

if ~isempty(results_uncal)
    text(0.05, y_pos, 'Uncalibrated:', 'FontSize', 10, 'FontWeight', 'bold', 'Color', [0.8 0.2 0.2]);
    text(0.1, y_pos-0.05, sprintf('Mean: %.2f° ± %.2f°', mean(all_frame_errors_uncal), std(all_frame_errors_uncal)), 'FontSize', 8);
    text(0.1, y_pos-0.10, sprintf('Median: %.2f°', median(all_frame_errors_uncal)), 'FontSize', 8);
end

method_title = upper(strrep(aoa_method, '_', ' '));
sgtitle(sprintf('Multi-Angle Calibration Test Results (Multi-Config, Method: %s)', method_title), ...
    'FontSize', 15, 'FontWeight', 'bold');

saveas(gcf, 'multi_angle_results_multi_config.png');
fprintf('Saved: multi_angle_results_multi_config.png\n\n');

% Save per-frame errors to CSV for Python CDF plotting
fprintf('Saving per-frame errors to CSV for Python CDF plotting...\n');
if ~isempty(all_frame_errors_8port_filtered)
    T_8port = table(all_frame_errors_8port_filtered, 'VariableNames', {'Absolute_Error'});
    writetable(T_8port, 'frame_errors_8port_filtered.csv');
    fprintf('  Saved: frame_errors_8port_filtered.csv (N=%d)\n', length(all_frame_errors_8port_filtered));
end
if ~isempty(all_frame_errors_4port_filtered)
    T_4port = table(all_frame_errors_4port_filtered, 'VariableNames', {'Absolute_Error'});
    writetable(T_4port, 'frame_errors_4port_filtered.csv');
    fprintf('  Saved: frame_errors_4port_filtered.csv (N=%d)\n', length(all_frame_errors_4port_filtered));
end
if ~isempty(all_frame_errors_2port_filtered)
    T_2port = table(all_frame_errors_2port_filtered, 'VariableNames', {'Absolute_Error'});
    writetable(T_2port, 'frame_errors_2port_filtered.csv');
    fprintf('  Saved: frame_errors_2port_filtered.csv (N=%d)\n', length(all_frame_errors_2port_filtered));
end
fprintf('\n');

% Save per-angle statistics to CSV for Python error plotting
fprintf('Saving per-angle statistics to CSV for Python error plotting...\n');
if ~isempty(results_8port)
    angles_8port = [results_8port.angle]';
    T_8port_angle = table(angles_8port, error_8port_filtered, std_8port_filtered, ...
        mean_aoa_8port_filtered, std_aoa_8port_filtered, ...
        'VariableNames', {'True_Angle', 'Mean_Error', 'Std_Error', 'Estimated_Angle_Mean', 'Estimated_Angle_Std'});
    writetable(T_8port_angle, 'angle_errors_8port_filtered.csv');
    fprintf('  Saved: angle_errors_8port_filtered.csv (N=%d angles)\n', length(angles_8port));
end
if ~isempty(results_4port)
    angles_4port = [results_4port.angle]';
    T_4port_angle = table(angles_4port, error_4port_filtered, std_4port_filtered, ...
        mean_aoa_4port_filtered, std_aoa_4port_filtered, ...
        'VariableNames', {'True_Angle', 'Mean_Error', 'Std_Error', 'Estimated_Angle_Mean', 'Estimated_Angle_Std'});
    writetable(T_4port_angle, 'angle_errors_4port_filtered.csv');
    fprintf('  Saved: angle_errors_4port_filtered.csv (N=%d angles)\n', length(angles_4port));
end
if ~isempty(results_2port)
    angles_2port = [results_2port.angle]';
    T_2port_angle = table(angles_2port, error_2port_filtered, std_2port_filtered, ...
        mean_aoa_2port_filtered, std_aoa_2port_filtered, ...
        'VariableNames', {'True_Angle', 'Mean_Error', 'Std_Error', 'Estimated_Angle_Mean', 'Estimated_Angle_Std'});
    writetable(T_2port_angle, 'angle_errors_2port_filtered.csv');
    fprintf('  Saved: angle_errors_2port_filtered.csv (N=%d angles)\n', length(angles_2port));
end
if ~isempty(results_uncal)
    angles_uncal = [results_uncal.angle]';
    errors_uncal = [results_uncal.error_uncal]';
    stds_uncal = [results_uncal.aoa_uncal_std]';
    T_uncal_angle = table(angles_uncal, errors_uncal, stds_uncal, ...
        'VariableNames', {'True_Angle', 'Mean_Error', 'Std_Error'});
    writetable(T_uncal_angle, 'angle_errors_uncal.csv');
    fprintf('  Saved: angle_errors_uncal.csv (N=%d angles)\n', length(angles_uncal));
end
fprintf('\n');

fprintf('========================================\n');
fprintf('   TEST COMPLETE\n');
fprintf('========================================\n');

