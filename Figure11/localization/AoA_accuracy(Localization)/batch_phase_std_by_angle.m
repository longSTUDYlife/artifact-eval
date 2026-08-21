clear; clc;

datapath = fileparts(mfilename('fullpath'));
numCIRPoints = 992;
firstCIRTap  = 0;
CIR_OVERSAMPLE_RATE = 64;

aoa_list = -50:10:50;
port_pairs = {[3,4], [4,5]};
ports_needed = unique([port_pairs{:}]);

fprintf('%-6s', 'aoa');
for k = 1:numel(port_pairs)
    pp = port_pairs{k};
    fprintf(' | [%d,%d] N     std_rad   std_deg', pp(1), pp(2));
end
fprintf('\n');
fprintf('%s\n', repmat('-', 1, 6 + numel(port_pairs)*34));

results = struct('aoa', {}, 'pair', {}, 'N', {}, 'std_rad', {}, 'std_deg', {});
row = 0;

for ia = 1:numel(aoa_list)
    aoa = aoa_list(ia);

    complex_signals = cell(1, 8);
    seqList_all = cell(1, 8);
    firstPath_all = cell(1, 8);
    rxPreamCount_all = cell(1, 8);
    seqList_global_all = cell(1, 8);

    for portIdx = ports_needed
        csvFiles = fullfile(datapath, ...
            sprintf('antenna_data_port%d_8ports_concurrent_localization_aoa_accuracy_%d.csv', portIdx, aoa));
        if ~isfile(csvFiles)
            error('Missing file: %s', csvFiles);
        end
        data = readmatrix(csvFiles, 'NumHeaderLines', 1);
        seqList_all{portIdx} = data(:, 1);
        firstPath_all{portIdx} = data(:, 10);
        rxPreamCount_all{portIdx} = data(:, 9);
        [real1, imag1] = parseCIR(data, 11, numCIRPoints);
        complex_signals{portIdx} = real1 + 1i * imag1;

        seq = seqList_all{portIdx};
        g = zeros(size(seq));
        g(1) = seq(1);
        cycle_idx = 0;
        for i = 2:numel(seq)
            if seq(i) <= seq(i-1)
                cycle_idx = cycle_idx + 1;
            end
            g(i) = 256 * cycle_idx + seq(i);
        end
        seqList_global_all{portIdx} = g;
    end

    fprintf('%-6d', aoa);
    for k = 1:numel(port_pairs)
        PortPair = port_pairs{k};
        phase_diff_list = compute_phase_diff_list( ...
            PortPair, complex_signals, seqList_global_all, ...
            firstPath_all, rxPreamCount_all, firstCIRTap, CIR_OVERSAMPLE_RATE);
        phase_std = std(phase_diff_list);
        fprintf(' | %6d  %8.6f  %7.4f', numel(phase_diff_list), phase_std, rad2deg(phase_std));

        row = row + 1;
        results(row).aoa = aoa;
        results(row).pair = PortPair;
        results(row).N = numel(phase_diff_list);
        results(row).std_rad = phase_std;
        results(row).std_deg = rad2deg(phase_std);
    end
    fprintf('\n');
end

% Summary: mean std across angles
fprintf('\nMean std across angles:\n');
for k = 1:numel(port_pairs)
    pp = port_pairs{k};
    mask = arrayfun(@(r) isequal(r.pair, pp), results);
    vals = [results(mask).std_rad];
    fprintf('  PortPair=[%d,%d]: mean_std=%.6f rad (%.4f deg)\n', ...
        pp(1), pp(2), mean(vals), rad2deg(mean(vals)));
end

out_csv = fullfile(datapath, 'phase_std_by_angle_34_45.csv');
aoa_col = [results.aoa].';
pair_str = arrayfun(@(r) sprintf('%d-%d', r.pair(1), r.pair(2)), results, 'UniformOutput', false).';
N_col = [results.N].';
std_rad_col = [results.std_rad].';
std_deg_col = [results.std_deg].';
T = table(aoa_col, pair_str, N_col, std_rad_col, std_deg_col, ...
    'VariableNames', {'aoa', 'port_pair', 'N', 'std_rad', 'std_deg'});
writetable(T, out_csv);
fprintf('\nSaved: %s\n', out_csv);

%% ===== helpers =====
function phase_diff_list = compute_phase_diff_list(PortPair, complex_signals, seqList_global_all, ...
        firstPath_all, rxPreamCount_all, firstCIRTap, CIR_OVERSAMPLE_RATE)
    Nframe = min(size(complex_signals{PortPair(1)}, 1), size(complex_signals{PortPair(2)}, 1));
    frameIdx_port2 = 1;
    phase_diff_list = [];
    for frameIdx = 1:Nframe
        while frameIdx_port2 <= numel(seqList_global_all{PortPair(2)}) && ...
                seqList_global_all{PortPair(1)}(frameIdx) > seqList_global_all{PortPair(2)}(frameIdx_port2)
            frameIdx_port2 = frameIdx_port2 + 1;
        end
        if frameIdx_port2 > numel(seqList_global_all{PortPair(2)})
            break;
        end
        if seqList_global_all{PortPair(1)}(frameIdx) ~= seqList_global_all{PortPair(2)}(frameIdx_port2)
            continue;
        end
        frameIndices_aligned = [frameIdx, frameIdx_port2];

        complex_fp_phase_one_stage_aligned = complex(zeros(1, 2));
        valid = true;
        for idx = 1:2
            portIdx = PortPair(idx);
            complex_signal = complex_signals{portIdx}(frameIndices_aligned(idx), :);
            complex_signal_upsampled = resample(complex_signal, CIR_OVERSAMPLE_RATE, 1) / ...
                rxPreamCount_all{portIdx}(frameIndices_aligned(idx));

            fp = firstPath_all{portIdx}(frameIndices_aligned(idx)) - firstCIRTap;
            fp_upsampled = round(fp * CIR_OVERSAMPLE_RATE);

            complex_signal_copy = complex_signal;
            fpindex_win = 1:min(numel(complex_signal_copy), round(fp) + 20);
            complex_signal_copy(fpindex_win) = 0;
            secondary_fpindex = Decavewave_LDE_v2(complex_signal_copy);
            secondary_fpindex_upsampled = round(secondary_fpindex * CIR_OVERSAMPLE_RATE);

            if fp_upsampled < 1 || fp_upsampled > numel(complex_signal_upsampled) || ...
               secondary_fpindex_upsampled < 1 || secondary_fpindex_upsampled > numel(complex_signal_upsampled)
                valid = false;
                break;
            end
            den = complex_signal_upsampled(secondary_fpindex_upsampled);
            if den == 0
                valid = false;
                break;
            end
            complex_fp_phase_one_stage_aligned(idx) = ...
                complex_signal_upsampled(fp_upsampled) / den;
        end

        if valid
            phase_diff_list(end+1) = angle( ...
                complex_fp_phase_one_stage_aligned(1) / complex_fp_phase_one_stage_aligned(2)); %#ok<SAGROW>
        end
        frameIdx_port2 = frameIdx_port2 + 1;
    end
end

function [realMat, imagMat] = parseCIR(data, cirStartCol, numCIRPoints)
    N = size(data, 1);
    cols = cirStartCol + (0:2*numCIRPoints-1);
    block = data(:, cols);
    realMat = block(:, 1:2:end);
    imagMat = block(:, 2:2:end);
end

function fp_index = Decavewave_LDE_v2(complex_signal)
    re = abs(real(complex_signal));
    im = abs(imag(complex_signal));
    amplitude = max([re; im]) + 0.25 * min([re; im]);
    [~, max_amp_index] = max(amplitude);
    gradient = [amplitude(2:end) - amplitude(1:end-1), 0];
    [~, max_g_index] = max(gradient(1:max_amp_index));
    if max_g_index <= 1 || max_g_index >= numel(gradient)
        fp_index = max_g_index;
        return;
    end
    denom = gradient(max_g_index) - min(gradient(max_g_index-1), gradient(max_g_index+1));
    if denom == 0
        frac_ts = 0;
    else
        frac_ts = 0.5 * (gradient(max_g_index+1) - gradient(max_g_index-1)) / denom;
    end
    fp_index = max_g_index + frac_ts - 1 + 0.5;
end
