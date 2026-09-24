import numpy as np
from scipy.sparse import lil_matrix, csr_matrix
from scipy.sparse.linalg import lsqr
import csv
import sys
import json

# ---------------------------------------------------------------
# 0. Constants
# ---------------------------------------------------------------
Z0 = {0: 0.0, 1: 24.0, 2: 375.0, 3: 399.0, 4: 490.5, 5: 510.0}  # nominal (design) Z positions
X_LAYERS = [0, 2, 4, 5]
Y_LAYERS = [1, 3, 4, 5]
ALL_LAYERS = sorted(set(X_LAYERS) | set(Y_LAYERS))

# ---------------------------------------------------------------
# 1. Parse & group into events (6 layers -> 8 lines/event: 0X,1Y,2X,3Y,4X,4Y,5X,5Y)
# ---------------------------------------------------------------
def parse(path):
    events = []
    cur = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            _, layer, axis, val = line.split(',')
            layer = int(layer)
            val = float(val)
            # a new event starts when we see layer 0 again after having filled some data
            if layer == 0 and axis == 'X' and cur:
                events.append(cur)
                cur = {}
            key = (layer, axis)
            cur[key] = val
    if cur:
        events.append(cur)
    return events

HITS_PATH = sys.argv[1] if len(sys.argv) > 1 else 'HITS.txt'
events = parse(HITS_PATH)
print(f"Parsed {len(events)} events")

def get(ev, layer, axis):
    v = ev.get((layer, axis))
    if v is None or v == -100:
        return None
    return v

# ---------------------------------------------------------------
# 2. Build & solve the global LINEAR sub-problem for one axis (X or Y),
#    for a GIVEN set of Z positions (Z_used). This is linear in
#    (dx[L] or dy[L]) and per-event line params (a_i, b_i):
#
#      value = a_i + b_i*Z_used[L] + d[L]
#
#    Gauge fix: sum(d) = 0 (heavy weight), since d and a_i are degenerate
#    (shifting all d by c and all a_i by -c leaves residuals unchanged).
# ---------------------------------------------------------------
def build_and_solve(events, layers, axis, Z_used, gauge_weight=1.0e4):
    nL = len(layers)
    layer_idx = {L: i for i, L in enumerate(layers)}

    # collect usable events (>=2 valid hits on this axis)
    obs = []  # (event_id, layer, z, value)
    for i, ev in enumerate(events):
        pts = [(L, Z_used[L], get(ev, L, axis)) for L in layers if get(ev, L, axis) is not None]
        if len(pts) >= 2:
            for (L, z, v) in pts:
                obs.append((i, L, z, v))

    used_set = sorted(set(o[0] for o in obs))
    ev_compact = {eid: k for k, eid in enumerate(used_set)}
    nE = len(used_set)

    n_obs = len(obs)
    n_unknown = nL + 2 * nE  # d[0..nL-1], then (a_i,b_i) for each used event
    n_rows = n_obs + 1  # +1 gauge-fix row

    A = lil_matrix((n_rows, n_unknown))
    b = np.zeros(n_rows)

    for r, (eid, L, z, v) in enumerate(obs):
        e = ev_compact[eid]
        A[r, layer_idx[L]] = 1.0          # d[L]
        A[r, nL + 2*e] = 1.0               # a_i
        A[r, nL + 2*e + 1] = z             # b_i * z
        b[r] = v

    for L in layers:
        A[n_obs, layer_idx[L]] = gauge_weight
    b[n_obs] = 0.0

    A = csr_matrix(A)
    sol = lsqr(A, b, atol=1e-12, btol=1e-12, iter_lim=200000)[0]

    d = sol[:nL]
    d_dict = {L: d[layer_idx[L]] for L in layers}

    lines = {}
    for eid in used_set:
        e = ev_compact[eid]
        a_i = sol[nL + 2*e]
        b_i = sol[nL + 2*e + 1]
        lines[eid] = (a_i, b_i)

    return d_dict, lines, obs, ev_compact

# ---------------------------------------------------------------
# 2b. Update Z offsets dz[L], given current dx/dy fits (a_i, b_i fixed).
#     Linearization: value - offset - a_i - b_i*(Z0[L] + dz[L] + ddz)
#                  = r0 - b_i*ddz          where r0 is residual at current dz
#
#     A pure least-squares update here is only weakly identifiable:
#     if the slope b_i is similar across events, b_i*dz[L] is nearly
#     degenerate with dx[L] itself (a "flat valley"), and the iteration
#     can diverge to unphysically large |dz|. Since the problem states
#     the systematic Z uncertainty is of order 1-2 mm, we fold that in
#     as a Gaussian prior (Tikhonov / ridge regularization) on dz:
#
#         minimize  sum_i (r0_i - b_i*ddz)^2  +  (dz_current+ddz)^2 / sigma_prior^2
#
#     => ddz[L] = ( sum(b_i*r0_i) - dz_current[L]/sigma_prior^2 )
#                 / ( sum(b_i^2) + 1/sigma_prior^2 )
#
#     Layers 4,5 get contributions from BOTH the X-fit and the Y-fit,
#     since they share the same physical Z position.
#     Reference layer (layer 0) is kept fixed at dz=0 to remove the
#     remaining gauge freedom (a uniform Z shift is fully absorbed by a_i).
# ---------------------------------------------------------------
def update_dz(obs, ev_compact, lines, d_dict, accum_num, accum_den):
    for (eid, L, z_used, v) in obs:
        a_i, b_i = lines[eid]
        r0 = v - d_dict[L] - a_i - b_i * z_used  # residual at current dz (z_used already includes it)
        accum_num[L] = accum_num.get(L, 0.0) + b_i * r0
        accum_den[L] = accum_den.get(L, 0.0) + b_i * b_i

# ---------------------------------------------------------------
# 3. Outer iterative loop: alternately solve (dx,dy | fixed dz) and
#    (dz | fixed dx,dy,lines). Reference layer 0 held at dz=0.
# ---------------------------------------------------------------
REF_LAYER = 0
N_ITER = 30
DAMPING = 0.7          # damping factor on dz updates for stability
DZ_PRIOR_SIGMA = 2.0   # mm; Gaussian-prior width reflecting the stated 1-2mm systematic uncertainty

dz = {L: 0.0 for L in ALL_LAYERS}

for it in range(N_ITER):
    Z_used = {L: Z0[L] + dz[L] for L in ALL_LAYERS}

    dx, lines_x, obs_x, evc_x = build_and_solve(events, X_LAYERS, 'X', Z_used)
    dy, lines_y, obs_y, evc_y = build_and_solve(events, Y_LAYERS, 'Y', Z_used)

    accum_num, accum_den = {}, {}
    update_dz(obs_x, evc_x, lines_x, dx, accum_num, accum_den)
    update_dz(obs_y, evc_y, lines_y, dy, accum_num, accum_den)

    max_change = 0.0
    for L in ALL_LAYERS:
        if L == REF_LAYER:
            continue
        if accum_den.get(L, 0.0) > 1e-12:
            prior_prec = 1.0 / DZ_PRIOR_SIGMA**2
            ddz = (accum_num[L] - dz[L] * prior_prec) / (accum_den[L] + prior_prec)
            dz[L] += DAMPING * ddz
            max_change = max(max_change, abs(DAMPING * ddz))

    print(f"[iter {it+1:2d}] dz = " +
          ", ".join(f"{L}:{dz[L]:+.4f}" for L in ALL_LAYERS) +
          f"   (max update {max_change:.2e})")

    if max_change < 1e-6:
        print(f"Converged after {it+1} iterations.")
        break

# final solve with converged Z_used, to get final dx, dy, lines consistent with dz
Z_used = {L: Z0[L] + dz[L] for L in ALL_LAYERS}
dx, lines_x, obs_x, evc_x = build_and_solve(events, X_LAYERS, 'X', Z_used)
dy, lines_y, obs_y, evc_y = build_and_solve(events, Y_LAYERS, 'Y', Z_used)

# ---------------------------------------------------------------
# 4. Residual RMS before / after correction
#    "before" = nominal Z0, no dx/dy/dz correction at all
#    "after"  = estimated dx, dy, dz all applied
# ---------------------------------------------------------------
def residual_rms(obs_nominal, ev_compact, lines, d_dict, corrected):
    res = []
    for (eid, L, z_used, v) in obs_nominal:
        a_i, b_i = lines[eid]
        if corrected:
            pred = a_i + b_i * z_used
            val = v - d_dict[L]
        else:
            pred = a_i + b_i * Z0[L]
            val = v
        res.append(val - pred)
    res = np.array(res)
    return np.sqrt(np.mean(res**2))

rms_x_before = residual_rms(obs_x, evc_x, lines_x, dx, corrected=False)
rms_x_after  = residual_rms(obs_x, evc_x, lines_x, dx, corrected=True)
rms_y_before = residual_rms(obs_y, evc_y, lines_y, dy, corrected=False)
rms_y_after  = residual_rms(obs_y, evc_y, lines_y, dy, corrected=True)

# ---------------------------------------------------------------
# 5. Report
# ---------------------------------------------------------------
print("\n=== Estimated layer offsets (dx, dy) ===")
for L in X_LAYERS:
    print(f"dx[{L}] = {dx[L]:+.6f}")
for L in Y_LAYERS:
    print(f"dy[{L}] = {dy[L]:+.6f}")

print("\n=== Estimated Z offsets (dz), relative to layer0 = 0 ===")
for L in ALL_LAYERS:
    print(f"dz[{L}] = {dz[L]:+.6f}   (Z_used = {Z_used[L]:.4f}, nominal {Z0[L]:.4f})")

print("\n=== Residual RMS (line-fit residual) ===")
print(f"X: before = {rms_x_before:.6f}   after = {rms_x_after:.6f}   (reduction {100*(1-rms_x_after/rms_x_before):.1f}%)")
print(f"Y: before = {rms_y_before:.6f}   after = {rms_y_after:.6f}   (reduction {100*(1-rms_y_after/rms_y_before):.1f}%)")

# dx0 / dy1 を基準（=0）にした相対オフセット
# （ゲージ自由度: 全dxに定数を足しても残差は不変なので、dx[0]を引いて基準化する）
# 注: layer0にはY測定が存在しない(Y_LAYERS=[1,3,4,5])ため、Y側はdy1を基準にする
dx_rel0 = {L: v - dx[0] for L, v in dx.items()}
dy_rel0 = {L: v - dy[1] for L, v in dy.items()}

print("\n=== Offsets relative to layer0/layer1 baseline (dx0=0, dy1=0) ===")
for L in X_LAYERS:
    print(f"dx_rel0[{L}] = {dx_rel0[L]:+.6f}")
for L in Y_LAYERS:
    print(f"dy_rel0[{L}] = {dy_rel0[L]:+.6f}")
print("\n(dz is already relative to layer0=0 by construction, no further baselining needed)")

# save results
result = {
    "dx": dx, "dy": dy,
    "dx_rel0": dx_rel0, "dy_rel0": dy_rel0,
    "dz": dz,
    "Z0": Z0, "Z_used": Z_used,
    "rms_x_before": rms_x_before, "rms_x_after": rms_x_after,
    "rms_y_before": rms_y_before, "rms_y_after": rms_y_after,
    "n_events_total": len(events),
    "n_events_used_x": len(evc_x),
    "n_events_used_y": len(evc_y),
}
with open('result.json', 'w') as f:
    json.dump(result, f, indent=2)
print("\nSaved result.json")

# ---------------------------------------------------------------
# 6. Visualization: residual histograms (before/after) & example event fits
# ---------------------------------------------------------------
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def residuals(obs_nominal, ev_compact, lines, d_dict, corrected):
    res = []
    for (eid, L, z_used, v) in obs_nominal:
        a_i, b_i = lines[eid]
        if corrected:
            pred = a_i + b_i * z_used
            val = v - d_dict[L]
        else:
            pred = a_i + b_i * Z0[L]
            val = v
        res.append(val - pred)
    return np.array(res)

rx_before = residuals(obs_x, evc_x, lines_x, dx, False)
rx_after  = residuals(obs_x, evc_x, lines_x, dx, True)
ry_before = residuals(obs_y, evc_y, lines_y, dy, False)
ry_after  = residuals(obs_y, evc_y, lines_y, dy, True)

fig, axes = plt.subplots(2, 2, figsize=(11, 8))
bins = np.linspace(-4, 4, 60)
axes[0, 0].hist(rx_before, bins=bins, color='#d9544d', alpha=0.8)
axes[0, 0].set_title(f"X residuals (before), RMS={rx_before.std():.3f}")
axes[0, 1].hist(rx_after, bins=bins, color='#4d8fd9', alpha=0.8)
axes[0, 1].set_title(f"X residuals (after), RMS={rx_after.std():.3f}")
axes[1, 0].hist(ry_before, bins=bins, color='#d9544d', alpha=0.8)
axes[1, 0].set_title(f"Y residuals (before), RMS={ry_before.std():.3f}")
axes[1, 1].hist(ry_after, bins=bins, color='#4d8fd9', alpha=0.8)
axes[1, 1].set_title(f"Y residuals (after), RMS={ry_after.std():.3f}")
for ax in axes.flat:
    ax.set_xlabel("residual")
    ax.set_ylabel("count")
plt.tight_layout()
plt.savefig('residuals_hist.png', dpi=130)
plt.close(fig)
print("Saved residuals_hist.png")

# pick up to N_SAMPLE events that have all X and all Y hits valid, for illustration
N_SAMPLE = 100
sample_ids = []
for i, ev in enumerate(events):
    xs = [get(ev, L, 'X') for L in X_LAYERS]
    ys = [get(ev, L, 'Y') for L in Y_LAYERS]
    if all(v is not None for v in xs) and all(v is not None for v in ys):
        sample_ids.append(i)
    if len(sample_ids) >= N_SAMPLE:
        break

if sample_ids:
    fig, axes = plt.subplots(2, 1, figsize=(7, 10))
    zs_x_nom = [Z0[L] for L in X_LAYERS]
    zs_y_nom = [Z0[L] for L in Y_LAYERS]
    zs_x_used = [Z_used[L] for L in X_LAYERS]
    zs_y_used = [Z_used[L] for L in Y_LAYERS]

    for n, i in enumerate(sample_ids):
        ev = events[i]
        xs_raw = [get(ev, L, 'X') for L in X_LAYERS]
        xs_corr = [get(ev, L, 'X') - dx[L] for L in X_LAYERS]
        ys_raw = [get(ev, L, 'Y') for L in Y_LAYERS]
        ys_corr = [get(ev, L, 'Y') - dy[L] for L in Y_LAYERS]

        lbl_before = 'raw (nominal Z, before)' if n == 0 else None
        lbl_after = 'corrected (dx/dy/dz, after)' if n == 0 else None

        # "before" plotted at nominal Z, "after" plotted at Z_used (Z0+dz)
        axes[0].plot(zs_x_nom, xs_raw, '-', color='red', alpha=0.3, linewidth=1, label=lbl_before)
        axes[0].plot(zs_x_used, xs_corr, '-', color='blue', alpha=0.3, linewidth=1, label=lbl_after)

        axes[1].plot(zs_y_nom, ys_raw, '-', color='red', alpha=0.3, linewidth=1, label=lbl_before)
        axes[1].plot(zs_y_used, ys_corr, '-', color='blue', alpha=0.3, linewidth=1, label=lbl_after)

    axes[0].set_title(f"X: {len(sample_ids)} events overlaid (raw=red, corrected=blue)")
    axes[0].set_xlabel("Z")
    axes[0].set_ylabel("X")
    axes[0].legend()

    axes[1].set_title(f"Y: {len(sample_ids)} events overlaid (raw=red, corrected=blue)")
    axes[1].set_xlabel("Z")
    axes[1].set_ylabel("Y")
    axes[1].legend()

    plt.tight_layout()
    plt.savefig('example_events.png', dpi=130)
    plt.close(fig)
    print(f"Saved example_events.png ({len(sample_ids)} events overlaid)")
else:
    print("No event with all X/Y hits valid found; skipped example_events.png")

