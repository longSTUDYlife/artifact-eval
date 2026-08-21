import os
import numpy as np
from load import parse
from algo import cir

DATA = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data', 'example_data')
zero_file = os.path.join(DATA, 'uloc_zero_0_20210427_181016_207A357A4653.log')

print(f"Loading {zero_file} ...")
ap_zero = parse.load_log(zero_file, interp_cir=8, suppress=True)
print(f"Parsed {len(ap_zero)} raw packets (before tag filter)")

ap_zero = parse.reformat_ap_data(ap_data=ap_zero, interp_cir=8, select_tag_addr='0000')
ap_zero = cir.extract_fp(ap_zero)

cir_fp = ap_zero['cir_fp']  # n_packets x 8 antennas, SFD-corrected first-path complex sample
n_packets = cir_fp.shape[0]
print(f"Usable packets after tag filter + reformat: {n_packets}")

# Relative phase of each antenna vs antenna 0 (this is exactly the "raw PDOA" analog:
# phase difference between two antennas fed by the shared reference clock chain)
phase_rel_deg = np.degrees(np.angle(cir_fp / cir_fp[:, [0]]))

print("\nPhase stability (antenna i vs antenna 0), over the whole 'zero' calibration recording:")
print(f"{'ant':>4} {'n':>6} {'mean(deg)':>10} {'std(deg)':>10} {'std(rad)':>10}")
for ant in range(1, 8):
    vals = phase_rel_deg[:, ant]
    mean = np.mean(vals)
    std = np.std(vals, ddof=1)
    print(f"{ant:>4} {len(vals):>6} {mean:>10.3f} {std:>10.3f} {np.radians(std):>10.5f}")

np.save(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'phase_rel_deg.npy'), phase_rel_deg)
print("\nSaved per-packet relative-phase matrix to phase_rel_deg.npy (n_packets x 8)")
