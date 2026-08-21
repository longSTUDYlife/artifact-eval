% Figure 13(e): env1 sensing trajectory from the same CIR as Figure10c.
% Raw: Figures/Done/Figure13e/raw -> Figure10c/raw (202603122 env1)
clear; clc;
set(0, 'DefaultFigureVisible', 'off');

this_dir = fileparts(mfilename('fullpath'));
cd(this_dir);
addpath(this_dir);

batch_extract_aoa('ports', 8);
fprintf('\nPlot: python plot_figure13e.py\n');
