function export_lde_complex_8antenna(inCsv, outCsv)
% 从 DW1000 宽表CSV 读取原始CIR，输出复数值而不是相位
% 这是专门为8天线阵列设计的版本

if nargin < 1 || isempty(inCsv)
    error('export_lde_complex_8antenna:NeedInput', 'Provide inCsv (CIR CSV path).');
end
if nargin < 2 || isempty(outCsv), outCsv = 'lde_two_complex_port1.csv'; end

% ===== 参数 =====
P.thFactor        = 6;
P.noiseFrac       = 0.15;
P.madK            = 4.5;
P.quantize64      = true;

P.gradWin         = 3;
P.ampWin          = 14;
P.mergeSeps       = 12;

P.schmittHigh     = 1.20;
P.schmittLookAhead= 10;
P.minStayBins     = 3;
P.minSlope        = 8;

P.ignoreRange     = [1, 2];
P.thAdd           = 300;

L = 64;
interpMethod = 'pchip';

% 期望的"索引差值"
[expectedGap, hasGapNum] = tail_number_from_filename(inCsv);
gapTol = 10;

% ===== 读取 CSV =====
T = readtable(inCsv);
names = T.Properties.VariableNames;
isR = startsWith(names,'CIR_real_');
isI = startsWith(names,'CIR_imag_');
rNames = names(isR); iNames = names(isI);

rIdx = sscanf(strjoin(erase(rNames,'CIR_real_'),' '),'%d'); [~,ordR]=sort(rIdx); rNames=rNames(ordR);
iIdx = sscanf(strjoin(erase(iNames,'CIR_imag_'),' '),'%d'); [~,ordI]=sort(iIdx); iNames=iNames(ordI);
assert(~isempty(rNames) && numel(rNames)==numel(iNames), '未找到匹配的 CIR_real_*/CIR_imag_* 列。');

Nbins = numel(rNames);
Nf    = height(T);

R = double(T{:, rNames});
I = double(T{:, iNames});
CIR = complex(R, I);

% ===== 主循环：逐帧 Top-2 LDE -> 复数值 =====
complex_small = nan(Nf,1) + 1j*nan(Nf,1);
complex_large = nan(Nf,1) + 1j*nan(Nf,1);

for k = 1:Nf
    x = CIR(k,:);
    mag = approx_mag(x);

    [candIdx, candAmp, ~] = lde_fullscan(mag, P);
    if isempty(candIdx)
        continue;
    end

    [~, srt] = sort(candAmp, 'descend');
    srt = srt(1:min(2,numel(srt)));
    ldes = candIdx(srt) + 0.5;

    if numel(ldes) < 2
        xi   = 1 : 1/L : Nbins;
        x_up = interp1(1:Nbins, x, xi, interpMethod, 'extrap');
        idx_up = @(lf) min(max(round((lf - 1)*L) + 1, 1), numel(x_up));
        v1 = x_up(idx_up(ldes(1))); complex_small(k) = v1;
        continue;
    end

    xi   = 1 : 1/L : Nbins;
    x_up = interp1(1:Nbins, x, xi, interpMethod, 'extrap');
    idx_up = @(lf) min(max(round((lf - 1)*L) + 1, 1), numel(x_up));

    [ldes_sorted, ~] = sort(ldes,'ascend');
    smallIdx = ldes_sorted(1);
    largeIdx = ldes_sorted(2);
    delta = largeIdx - smallIdx;

    doSwap = false;
    if hasGapNum
        if delta > (expectedGap + gapTol)
            doSwap = true;
        elseif abs(delta - expectedGap) <= gapTol
            doSwap = false;
        elseif delta < (expectedGap - gapTol)
            doSwap = true;
        else
            doSwap = false;
        end
    end

    vSmall = x_up(idx_up(smallIdx));
    vLarge = x_up(idx_up(largeIdx));

    if ~doSwap
        complex_small(k) = vSmall;
        complex_large(k) = vLarge;
    else
        complex_small(k) = vLarge;
        complex_large(k) = vSmall;
    end
end

% ===== 导出四列（实部和虚部） =====
complex_small_real = real(complex_small);
complex_small_imag = imag(complex_small);
complex_large_real = real(complex_large);
complex_large_imag = imag(complex_large);

outT = table(complex_small_real, complex_small_imag, complex_large_real, complex_large_imag, ...
    'VariableNames', {'complex_small_real','complex_small_imag','complex_large_real','complex_large_imag'});
writetable(outT, outCsv);
fprintf('Wrote %s (rows=%d)\n', outCsv, Nf);

end

% ----------------- 工具函数们 -----------------
function m = approx_mag(xc)
    I = abs(real(xc)); Q = abs(imag(xc));
    m = max(I, Q) + 0.25*min(I, Q);
end

function [candIdx, candAmp, thr] = lde_fullscan(x, P)
    N = numel(x);
    noiseRegionBins = 10;
    mergeGapBelow   = 2;
    minStayAbove    = 3;
    earlyPeakFrac   = 0.80;
    gradSpan        = max(1, P.gradWin);

    valid = true(1,N);
    if isfield(P,'ignoreRange') && ~isempty(P.ignoreRange)
        valid(P.ignoreRange(1):min(P.ignoreRange(2),N)) = false;
    end

    startIdx = find(valid,1,'first'); if isempty(startIdx), startIdx=1; end
    noiseEnd = min(N, startIdx + noiseRegionBins - 1);
    thr      = P.thFactor * mean(x(startIdx:noiseEnd));

    above = x >= thr;
    if mergeGapBelow > 0
        z = ~above; dz = diff([0 z 0]);
        zs = find(dz==1); ze = find(dz==-1)-1;
        for ii = 1:numel(zs)
            Lg = zs(ii); Rg = ze(ii); gapLen = Rg-Lg+1;
            leftOK  = (Lg-1)>=1 && above(Lg-1);
            rightOK = (Rg+1)<=N && above(Rg+1);
            if gapLen <= mergeGapBelow && leftOK && rightOK
                above(Lg:Rg) = true;
            end
        end
    end
    d  = diff([0 above 0]);
    st = find(d==1); en = find(d==-1)-1;
    if isempty(st)
        candIdx = []; candAmp = []; return;
    end

    g = diff(x);
    candIdx = [];
    candAmp = [];

    for c = 1:numel(st)
        s_bin = st(c); e_bin = en(c);

        stay_hi = min(e_bin, s_bin + max(0,minStayAbove-1));
        if sum(x(s_bin:stay_hi) >= thr) < minStayAbove
            continue;
        end

        [peakAmpMax, relp] = max(x(s_bin:e_bin));
        peakIdxMax = s_bin + relp - 1;

        locs = [];
        for i = max(s_bin+1,2) : min(e_bin-1,N-1)
            if x(i) >= x(i-1) && x(i) > x(i+1)
                locs(end+1) = i; %#ok<AGROW>
            end
        end
        locs = locs(x(locs) >= thr);

        strongMask = ~isempty(locs) & (x(locs) >= earlyPeakFrac * peakAmpMax);
        if any(strongMask)
            seed = locs(find(strongMask,1,'first'));
        elseif ~isempty(locs)
            seed = locs(1);
        else
            seed = peakIdxMax;
        end

        lo = max(2, s_bin - gradSpan);
        hi = min(N-1, s_bin + gradSpan);
        hi = min(hi, seed-1);
        if lo > hi
            lo = max(2, seed-1); hi = seed-1;
            if lo > hi, lo = max(2, s_bin); hi = lo; end
        end

        [~, rmax] = max(g(lo:hi));
        m = lo + rmax - 1;

        gm1 = g(max(m-1,1)); g0 = g(m); gp1 = g(min(m+1,N-1));
        denom = (g0 - min(gm1, gp1));
        if denom <= 0
            frac = 0;
        else
            frac = 0.5 * (gp1 - gm1) / denom;
            frac = max(-0.5, min(0.5, frac));
        end
        lde = m + frac;

        lde = min(lde, seed - 0.5);

        if isfield(P,'quantize64') && P.quantize64
            lde = round(lde*64)/64;
        end

        candIdx(end+1) = lde;        %#ok<AGROW>
        candAmp(end+1) = peakAmpMax; %#ok<AGROW>
    end
end


function [num, ok] = tail_number_from_filename(fname)
    tok = regexp(fname, '(\d+)(?=[^\d]*$)', 'tokens', 'once');
    if isempty(tok)
        num = NaN; ok = false;
    else
        num = str2double(tok{1});
        ok = isfinite(num);
    end
end

