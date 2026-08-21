clear;clc;
%% ========== 1) Read Two CSV Files and Parse Amplitude + Real + Imaginary Parts ==========

% Dataset switch:
%   'aoa_accuracy' -> CSVs next to this script (8-port AoA accuracy)
%   '20260803'     -> Figures/.../localization/20260803 (2-port concurrent test)
dataset = '20260803';

scriptDir = fileparts(mfilename('fullpath'));
numCIRPoints = 992;   % CIR_real_0 .. CIR_real_991
firstCIRTap  = 0;

sequenceCol  = 1;   % 1st column: Sequence
rxPreamCount = 9;
firstPathCol = 10;  % 10th column: firstPath
cirStartCol  = 11;  % CIR starts here: 992 real/imag pairs (CIR_real_0 ..)
CIR_OVERSAMPLE_RATE = 64;

switch dataset
    case '20260803'
        datapath = fullfile(scriptDir, '..', '20260803');
        NPort = 2;
        aoa = 0;  % label only (true AoA unknown for this test set)
        PortPair = [1, 2];
    otherwise  % 'aoa_accuracy'
        datapath = scriptDir;
        NPort = 8;
        aoa = 0;
        PortPair = [1, 2];
end

complex_signals = {};
seqList_all = {};
firstPath_all = {};
rxPreamCount_all = {};

%% load data
for portIdx = 1:NPort
    if strcmp(dataset, '20260803')
        csvFiles = fullfile(datapath, ...
            sprintf('antenna_data_port%d_2ports_concurrent_test.csv', portIdx));
    else
        csvFiles = fullfile(datapath, ...
            sprintf('antenna_data_port%d_8ports_concurrent_localization_aoa_accuracy_%d.csv', portIdx, aoa));
    end
    if ~isfile(csvFiles)
        error('Missing file: %s', csvFiles);
    end

    data = readmatrix(csvFiles, "NumHeaderLines", 1);

    % Read sequence and firstPath
    seqList1   = data(:, sequenceCol);
    firstPath1 = data(:, firstPathCol);
    rxPreamCount1 = data(:, rxPreamCount);
    
    % Parse CIR real, imaginary, and amplitude values
    [real1, imag1, amp1] = parseCIR(data, cirStartCol, numCIRPoints);
    complex_signals{end + 1}   = real1 + 1i*imag1;
    seqList_all{end + 1} = seqList1;
    firstPath_all{end + 1} = firstPath1;
    rxPreamCount_all{end + 1} = rxPreamCount1;
end

%% Global sequence number alignment
seqList_global_all = {};
for portIdx = 1:NPort
    seqList_global_all{portIdx} = [seqList_all{portIdx}(1)];
    cycle_idx = 0;
    for i = 2:length(seqList_all{portIdx})
        if seqList_all{portIdx}(i) <=  seqList_all{portIdx}(i-1)
            cycle_idx = cycle_idx + 1;
        end
        seqList_global_all{portIdx}(end + 1) =  256*cycle_idx + seqList_all{portIdx}(i);
    end
end

%% Phase difference
Nframe = min(size(complex_signals{PortPair(1)},1), size(complex_signals{PortPair(2)},1));
frameIdx_port2 = 1;
phase_diff_list = [];
for frameIdx = 1:Nframe
    % frame synchronization
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
    frameIndices_aligned = [frameIdx, frameIdx_port2]; % Store aligned frame indices
   
    complex_fp_phase_one_stage_aligned = [];
    for idx = 1:length(PortPair)
        portIdx = PortPair(idx);
        complex_signal = complex_signals{portIdx}(frameIndices_aligned(idx),:);
        complex_signal_upsampled = resample(complex_signal, CIR_OVERSAMPLE_RATE, 1) / rxPreamCount_all{portIdx}(frameIndices_aligned(idx));

        % Read the first LDE        
        fp = (firstPath_all{portIdx}(frameIndices_aligned(idx)) - firstCIRTap);
        fp_upsampled = round(fp * CIR_OVERSAMPLE_RATE);
        
        % find another LDE
        complex_signal_copy = complex_signal;
        fpindex_win = 1:round(fp)+20; 
        complex_signal_copy(fpindex_win) = 0;
        secondary_fpindex = Decavewave_LDE_v2(complex_signal_copy);
        secondary_fpindex_upsampled = round(secondary_fpindex * CIR_OVERSAMPLE_RATE);

        % phase alignment
        complex_fp_phase_one_stage_aligned(idx) = complex_signal_upsampled(fp_upsampled) / complex_signal_upsampled(secondary_fpindex_upsampled);
    end

    complex_fp_phase_two_stage_aligned = (complex_fp_phase_one_stage_aligned(1) / complex_fp_phase_one_stage_aligned(2));
    phase_diff = angle(complex_fp_phase_two_stage_aligned);
    phase_diff_list(end+1) = phase_diff; %#ok<SAGROW>

    frameIdx_port2 = frameIdx_port2 + 1;
end

%% Final std
phase_std = std(phase_diff_list);
fprintf('aoa=%d, PortPair=[%d,%d], N=%d, std=%.6f rad (%.4f deg)\n', ...
    aoa, PortPair(1), PortPair(2), numel(phase_diff_list), phase_std, rad2deg(phase_std));

%% ========== Function parseCIR ==========
function [realMat, imagMat, ampMat] = parseCIR(data, cirStartCol, numCIRPoints)
    N = size(data,1);
    realMat = zeros(N, numCIRPoints);
    imagMat = zeros(N, numCIRPoints);
    ampMat  = zeros(N, numCIRPoints);
    for i = 1:N
        for k = 1:numCIRPoints
            reVal = data(i, cirStartCol + 2*(k-1));
            imVal = data(i, cirStartCol + 2*(k-1) + 1);
            realMat(i,k) = reVal;
            imagMat(i,k) = imVal;
            ampMat(i,k)  = sqrt(double(reVal)^2 + double(imVal)^2);
        end
    end
end

function [fp_index] = Decavewave_LDE_v2(complex_signal)
    % amplitude approximation
    re = abs(real(complex_signal));
    im = abs(imag(complex_signal));
    amplitude = (max([re;im]) + 0.25*min([re;im]));
    [max_amp, max_amp_index] = max(amplitude);

    % LDE implmentation
    gradient = [amplitude(2:end) - amplitude(1:end-1) 0];
   
    % approximation of max gradient
    % frac_ts = 0.5(g[m+1]-g[m-1])/(g[m]-min(g[m-1],g[m+1]))   (frac_ts is the range -0.5 … 0.5)
    [max_g, max_g_index] = max(gradient(1:max_amp_index));
    frac_ts = 0.5*(gradient(max_g_index+1)-...
                   gradient(max_g_index-1))...
                  /(gradient(max_g_index)...
                  -min(gradient(max_g_index-1),...
                   gradient(max_g_index+1))); 

    fp_index = max_g_index + frac_ts - 1 + 0.5;
end


