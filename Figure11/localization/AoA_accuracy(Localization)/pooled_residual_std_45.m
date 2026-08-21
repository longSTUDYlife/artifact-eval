clear; clc;

datapath = fileparts(mfilename('fullpath'));
numCIRPoints = 992;
firstCIRTap  = 0;
CIR_OVERSAMPLE_RATE = 64;

aoa_list = -40:10:40;
PortPair = [4, 5];
ports_needed = PortPair;

residuals = [];
fprintf('%-6s %6s %10s %10s %10s\n', 'aoa', 'N', 'mean', 'std', 'std_deg');
fprintf('%s\n', repmat('-', 1, 48));

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
        data = readmatrix(csvFiles, 'NumHeaderLines', 1);
        seqList_all{portIdx} = data(:, 1);
        firstPath_all{portIdx} = data(:, 10);
        rxPreamCount_all{portIdx} = data(:, 9);
        cols = 11 + (0:2*numCIRPoints-1);
        block = data(:, cols);
        complex_signals{portIdx} = block(:, 1:2:end) + 1i * block(:, 2:2:end);

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

    phase_diff_list = compute_phase_diff_list( ...
        PortPair, complex_signals, seqList_global_all, ...
        firstPath_all, rxPreamCount_all, firstCIRTap, CIR_OVERSAMPLE_RATE);

    mu = mean(phase_diff_list);
    s = std(phase_diff_list);
    residuals = [residuals, phase_diff_list - mu]; %#ok<AGROW>
    fprintf('%-6d %6d %10.6f %10.6f %10.4f\n', aoa, numel(phase_diff_list), mu, s, rad2deg(s));
end

pooled = std(residuals);
fprintf('\nPortPair=[4,5], angles=-40:10:40 (exclude ±50)\n');
fprintf('pooled residual n   = %d\n', numel(residuals));
fprintf('pooled residual mean= %.3e  (should be ~0)\n', mean(residuals));
fprintf('pooled residual std = %.6f rad (%.4f deg)\n', pooled, rad2deg(pooled));

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

        one = complex(zeros(1, 2));
        valid = true;
        for idx = 1:2
            portIdx = PortPair(idx);
            complex_signal = complex_signals{portIdx}(frameIndices_aligned(idx), :);
            up = resample(complex_signal, CIR_OVERSAMPLE_RATE, 1) / ...
                rxPreamCount_all{portIdx}(frameIndices_aligned(idx));
            fp = firstPath_all{portIdx}(frameIndices_aligned(idx)) - firstCIRTap;
            fp_u = round(fp * CIR_OVERSAMPLE_RATE);

            cpy = complex_signal;
            cpy(1:min(numel(cpy), round(fp) + 20)) = 0;
            sec = Decavewave_LDE_v2(cpy);
            sec_u = round(sec * CIR_OVERSAMPLE_RATE);

            if fp_u < 1 || fp_u > numel(up) || sec_u < 1 || sec_u > numel(up) || up(sec_u) == 0
                valid = false;
                break;
            end
            one(idx) = up(fp_u) / up(sec_u);
        end
        if valid
            phase_diff_list(end+1) = angle(one(1) / one(2)); %#ok<SAGROW>
        end
        frameIdx_port2 = frameIdx_port2 + 1;
    end
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
