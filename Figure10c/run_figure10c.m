% Figure 10(c): env1 sensing AoA (sequence-sync + baseline extract)
clear; clc;
set(0, 'DefaultFigureVisible', 'off');

this_dir = fileparts(mfilename('fullpath'));
cd(this_dir);
addpath(this_dir);

batch_extract_aoa();
fprintf('\nPlot: python plot_figure10c.py\n');
