% Figure 10(d): two-reflector angular resolution (FFT RA slice)
clear; clc;
set(0, 'DefaultFigureVisible', 'off');

this_dir = fileparts(mfilename('fullpath'));
cd(this_dir);
addpath(this_dir);

batch_extract_slice();
fprintf('\nPlot: python plot_figure10d.py\n');
