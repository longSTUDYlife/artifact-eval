% modified from multipaht_phasediff.m

clear;clc;
%% ========== 1) Read Two CSV Files and Parse Amplitude + Real + Imaginary Parts ==========
datapath = "";
csvFile_port1 = fullfile(datapath, "cable_data_port1_fp_mp.csv");
csvFile_port2 = fullfile(datapath, "cable_data_port2_fp_mp.csv");

data_port1 = readmatrix(csvFile_port1, "NumHeaderLines", 1);
data_port2 = readmatrix(csvFile_port2, "NumHeaderLines", 1);

sequenceCol  = 1;   % 1st column: Sequence
rxPreamCount = 9;
firstPathCol = 10;  % 10th column: firstPath
cirStartCol  = 11;  % The following 200 columns are CIR (100 pairs of complex numbers)
numCIRPoints = 100;
firstCIRTap  = 700;
firstPathAmp1Col = 5;
firstPathAmp2Col = 7;
firstPathAmp3Col = 8;

% Read sequence and firstPath
seqList1   = data_port1(:, sequenceCol);
firstPath1 = data_port1(:, firstPathCol);
rxPreamCount1 = data_port1(:, rxPreamCount);
seqList2   = data_port2(:, sequenceCol);
firstPath2 = data_port2(:, firstPathCol);
rxPreamCount2 = data_port2(:, rxPreamCount);

firstPathAmp1_cable = data_port2(:,firstPathAmp1Col);
firstPathAmp2_cable = data_port2(:,firstPathAmp2Col);
firstPathAmp3_cable = data_port2(:,firstPathAmp3Col);

% Parse CIR real, imaginary, and amplitude values
[real1, imag1, amp1] = parseCIR(data_port1, cirStartCol, numCIRPoints);
[real2, imag2, amp2] = parseCIR(data_port2, cirStartCol, numCIRPoints);

numFrames = min([size(data_port1,1), size(data_port2,1)]);
complex_port1   = real1 + 1i*imag1;
complex_port2 = real2 + 1i*imag2;

% DSP
fs_fast  = 1e9;
c = 3e8;
fc = 3.4944e9;                   % center frequency (Hz)
lambda = c / fc;                % wavelength (m)
range_resolution = c / (2 * fs_fast);
fast_time_oversample = 64;
range_win_left = 10; range_win_right = 48; num_taps = range_win_right + range_win_left + 1;
win_left = fast_time_oversample * range_win_left; win_right = fast_time_oversample * range_win_right;
range_axis_oversample = (0:fast_time_oversample*(num_taps-1)) * range_resolution / fast_time_oversample;  % meters

correct_phase_port1_list  = [];
correct_phase_port2_list  = [];
uncorrect_phase_diff_list = [];
phase_diff_list = [];
frameIdx_port2 = 1;

nsamples_to_process = 300;
for frameIdx = 1:nsamples_to_process
    % frame synchronization
    while seqList1(frameIdx) > seqList2(frameIdx_port2)
        frameIdx_port2 = frameIdx_port2 + 1;
    end
    if seqList1(frameIdx) ~= seqList2(frameIdx_port2)
        continue;
    end
    % LDE
    % LDE of DW1000
    fpindex_port1 = firstPath1(frameIdx) - firstCIRTap;
    fpindex_port1_upsample = round(fpindex_port1 * fast_time_oversample);
    fpindex_port2 = firstPath2(frameIdx_port2) - firstCIRTap;
    fpindex_port2_upsample = round(fpindex_port2 * fast_time_oversample);
    
    if fpindex_port1 > numCIRPoints || fpindex_port2 > numCIRPoints
        continue  
    end

    % find another LDE
    complex_port1_ = complex_port1(frameIdx, :);
    fpindex_win_port1 = round(fpindex_port1)-20:round(fpindex_port1)+20; 
    complex_port1_(fpindex_win_port1) = 0;
    secondary_fpindex_port1 = Decavewave_LDE(complex_port1_);
    secondary_fpindex_port1_up = round(secondary_fpindex_port1 * 64);

    complex_port2_ = complex_port2(frameIdx_port2, :);
    fpindex_win_port2 = round(fpindex_port2)-20:round(fpindex_port2)+20; 
    complex_port2_(fpindex_win_port2) = 0;
    secondary_fpindex_port2 = Decavewave_LDE(complex_port2_);
    secondary_fpindex_port2_up = round(secondary_fpindex_port2 * 64);

    % Unsample signal
    complex_port1_upsampled = resample(complex_port1(frameIdx, :), 64, 1) / rxPreamCount1(frameIdx);
    complex_port2_upsampled = resample(complex_port2(frameIdx_port2, :), 64, 1) / rxPreamCount2(frameIdx);
  
    fp1_up = round((firstPath1(frameIdx) - firstCIRTap + 1) * 64);
    fp2_up = round((firstPath2(frameIdx_port2) - firstCIRTap + 1) * 64);
  
    amplitude_port1_normalized = abs(complex_port1_upsampled) / max(abs(complex_port1_upsampled));
    amplitude_port2_normalized = abs(complex_port2_upsampled) /  max(abs(complex_port2_upsampled));

    derivative_amplitude_port1 = [amplitude_port1_normalized(2:end) - amplitude_port1_normalized(1:end-1) 0];
    derivative_amplitude_port2 = [amplitude_port2_normalized(2:end) - amplitude_port2_normalized(1:end-1) 0];
    
    secondary_fpindex_port1_up = fp1_up + 1584;
    secondary_fpindex_port2_up = fp2_up + 1778;

    % phase diff
    fp1_small = min([fp1_up  secondary_fpindex_port1_up]);
    fp1_big   = max([fp1_up  secondary_fpindex_port1_up]);
    correct_phase_port1 = complex_port1_upsampled(fp1_big) / complex_port1_upsampled(fp1_small);
  
    fp2_small = min([fp2_up  secondary_fpindex_port2_up]);
    fp2_big   = max([fp2_up  secondary_fpindex_port2_up]);
    correct_phase_port2 = complex_port2_upsampled(fp2_big) / complex_port2_upsampled(fp2_small);
 
    uncorrect_phase_diff = angle(complex_port1_upsampled(fp1_big) / complex_port2_upsampled(fp2_big));
    phase_diff = angle(correct_phase_port1 / correct_phase_port2);
  
    uncorrect_phase_diff_list(end+1) = uncorrect_phase_diff;
    phase_diff_list(end + 1) = phase_diff;
    correct_phase_port1_list(end + 1)  = angle(correct_phase_port1);
    correct_phase_port2_list(end + 1)  = angle(correct_phase_port2);
 
    clf;
    subplot(4,1,1)
    plot(amplitude_port1_normalized);
    hold on;
    plot(fp1_up+1, amplitude_port1_normalized(fp1_up+1),'ro', 'MarkerFaceColor','r','MarkerSize',4)
    hold on;
    plot(secondary_fpindex_port1_up+1, amplitude_port1_normalized(secondary_fpindex_port1_up+1),'bo', 'MarkerFaceColor','b','MarkerSize',4)

    subplot(4,1,2)
    plot(derivative_amplitude_port1);
    hold on;
    plot(fp1_up+1, derivative_amplitude_port1(fp1_up+1),'ro', 'MarkerFaceColor','r','MarkerSize',4)
    hold on;
    plot(secondary_fpindex_port1_up+1, derivative_amplitude_port1(secondary_fpindex_port1_up+1),'bo', 'MarkerFaceColor','b','MarkerSize',4)

    subplot(4,1,3)
    plot(amplitude_port2_normalized)
    hold on;
    plot(fp2_up+1, amplitude_port2_normalized(fp2_up+1),'ro', 'MarkerFaceColor','r','MarkerSize',4)
    hold on;
    plot(secondary_fpindex_port2_up+1, amplitude_port2_normalized(secondary_fpindex_port2_up+1),'bo', 'MarkerFaceColor','b','MarkerSize',4)
    title(phase_diff)
    
    subplot(4,1,4)
    plot(derivative_amplitude_port2);
    hold on;
    plot(fp2_up+1, derivative_amplitude_port2(fp2_up+1),'ro', 'MarkerFaceColor','r','MarkerSize',4)
    hold on;
    plot(secondary_fpindex_port2_up+1, derivative_amplitude_port2(secondary_fpindex_port2_up+1),'bo', 'MarkerFaceColor','b','MarkerSize',4)

    frameIdx_port2 = frameIdx_port2 + 1;
end

%%
figure;
plot(rad2deg(uncorrect_phase_diff_list),'.-','Linewidth', 2)
hold on;
plot(rad2deg(phase_diff_list),'.-','Linewidth', 2)
xlabel("frame indices")
ylabel("Phase difference (Degree)")
grid on;
legend("Original", "Noise cancellation")
set(gca,'FontSize', 24)
title(sprintf("STD of corrected phase diff = %.3f", std(phase_diff_list)));

figure;
plot(rad2deg(correct_phase_port1_list),'.-','Linewidth', 2)
hold on;
plot(rad2deg(correct_phase_port2_list),'.-','Linewidth', 2)
xlabel("frame indices")
ylabel("Corrected Phase (Degree)")
grid on;
legend("Port1", "Port2")
set(gca,'FontSize', 24)
title(sprintf("STD of corrected phase, port1 = %.3f, port2 = %.3f", std(correct_phase_port1_list), std(correct_phase_port2_list)));

function [fp_index] = Decavewave_LDE(complex_signal)
    % amplitude approximation
    re = abs(real(complex_signal));
    im = abs(imag(complex_signal));
    amplitude = (max([re;im]) + 0.25*min([re;im]));

    % LDE implmentation
    gradient = [amplitude(2:end) - amplitude(1:end-1) 0];
   
    % approximation of max gradient
    % frac_ts = 0.5(g[m+1]-g[m-1])/(g[m]-min(g[m-1],g[m+1]))   (frac_ts is the range -0.5 … 0.5)
    [max_g, max_g_index] = max(gradient);
    frac_ts = 0.5*(gradient(max_g_index+1)-...
                   gradient(max_g_index-1))...
                  /(gradient(max_g_index)...
                  -min(gradient(max_g_index-1),...
                   gradient(max_g_index+1))); 

    fp_index = max_g_index + frac_ts - 1 + 0.5;
end

