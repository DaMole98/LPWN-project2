import numpy as np
import matplotlib.pyplot as plt

# -------------------- parameters --------------------
np.random.seed(123)
T = 300
total_transmissions = 60000
M = total_transmissions // T
depth = 10

# propagation params (indoor Firefly)
distances_h = np.linspace(5.0, 50.0, depth)
d0 = 20.0           # reference distance (m)
PL0 = 45.0          # path‑loss at d0 [dB]
n = 3.0             # path‑loss exponent
Ptx = 0.0           # TX power [dBm]
sigma_sh = 7.0      # shadowing st.dev. [dB]
rho = 0.9           # AR(1) correlation
sigma_fad = 1.0     # Rayleigh scale  (unused here)

# ------------- functions -----------------
def generate_rssi(T, d, d0, PL0, n, Ptx, sigma_sh, rho):
    """Large‑scale RSSI: path‑loss + shadowing (AR(1))."""
    PL_d = PL0 + 10 * n * np.log10(d / d0)
    shadow = np.zeros(T)
    for t in range(1, T):
        shadow[t] = rho * shadow[t-1] + np.sqrt(1 - rho**2) * np.random.normal(0, sigma_sh)
    return Ptx - PL_d + shadow

# linear mapping RSSI → pACK
def build_linear_mapping(rssi_matrix):
    rssi_min = rssi_matrix.min()
    rssi_max = rssi_matrix.max()
    def mapper(r):
        return np.clip((r - rssi_min) / (rssi_max - rssi_min), 0, 1)
    return mapper

# fallback ETX from RSSI
rssi_high, rssi_low = -30, -85
def etx_from_rssi(r):
    if r > rssi_high: return 1.0
    if r < rssi_low:  return 10.0
    return 1 + ((rssi_high - r) / (rssi_high - rssi_low)) * 9

# ------------- simulate RSSI -------------
RSSI_h = [generate_rssi(T, distances_h[d], d0, PL0, n, Ptx, sigma_sh, rho)
          for d in range(depth)]

# mapping
linear_map = build_linear_mapping(np.hstack(RSSI_h))
p_schedule_h = np.array([[linear_map(r) for r in rss] for rss in RSSI_h])

# add 5 % Gaussian noise
p_schedule_h = np.clip(p_schedule_h + np.random.normal(0, 0.05, p_schedule_h.shape), 0, 1)

# ACK counts
ACK_h = [np.random.binomial(M, p_schedule_h[d]) for d in range(depth)]
TX_h  = [np.full(T, M) for _ in range(depth)]

# ------------- ETX evolution for hop 0 -------------
alpha_vals = [0.0, 0.2, 0.5, 0.8, 1.0]
etx_ser = {}
d0_hop = 0
for alpha in alpha_vals:
    etx = np.zeros(T)
    etx[0] = etx_from_rssi(RSSI_h[d0_hop][0]) if ACK_h[d0_hop][0]==0 else TX_h[d0_hop][0]/ACK_h[d0_hop][0]
    for t in range(1, T):
        if ACK_h[d0_hop][t]==0 or alpha==1.0:
            etx[t] = etx_from_rssi(RSSI_h[d0_hop][t])
        else:
            etx[t] = alpha*etx[t-1] + (1-alpha)*(TX_h[d0_hop][t]/ACK_h[d0_hop][t])
    etx_ser[alpha] = etx

# ------------- plot ETX -------------
plt.figure(figsize=(12,5))
cols = plt.cm.plasma(np.linspace(0,1,len(alpha_vals)))
for c,a in zip(cols, alpha_vals):
    plt.plot(etx_ser[a], label=f'α={a}', color=c, linewidth=1.2)

seg_len = 30
segments_cnt = T//seg_len
for b in range(1, segments_cnt):
    plt.axvline(b*seg_len, color='grey', linestyle='--', linewidth=0.8)

ymax = plt.ylim()[1]
for i in range(segments_cnt):
    p_mean = p_schedule_h[d0_hop, i*seg_len:(i+1)*seg_len].mean()
    plt.text(i*seg_len+seg_len/2, ymax*0.9, f'$p_{{ACK}}={p_mean:.2f}$',
             ha='center', va='top', fontsize=8)
plt.title('ETX$_t$ evolution – hop 1')
plt.xlabel('Window index $t$')
plt.ylabel('ETX')
plt.grid(True, linestyle='--', linewidth=0.5)
plt.legend()
plt.tight_layout()
plt.savefig("etx_alpha.png")


# ===== plotting =====
plt.figure(figsize=(12,5))
for col, α in zip(plt.cm.plasma(np.linspace(0,1,len(alpha_vals))), alpha_vals):
    plt.plot(etx_ser[α], label=f'α={α}', color=col, lw=1.2)

seg_len = 30
for b in range(1, T//seg_len):
    plt.axvline(b*seg_len, ls='--', lw=.8, color='grey')

ymax = plt.ylim()[1]
for i in range(T//seg_len):
    p_mean = p_schedule_h[d0_hop, i*seg_len:(i+1)*seg_len].mean()
    plt.text(i*seg_len+seg_len/2, ymax*0.9,
             rf'$p_{{ACK}}={p_mean:.2f}$', ha='center', va='top', fontsize=8)

# --- 4) Calcolo metrica cumulativa M_v(t) su tutti i depth ---
alpha_M = 0.75
etx_h = np.zeros((depth, T))
for d in range(depth):
    etx_h[d, 0] = f_rssi(RSSI_h[d][0]) if ACK_h[d][0] == 0 else TX_h[d][0] / ACK_h[d][0]
    for t in range(1, T):
        if ACK_h[d][t] == 0:
            etx_h[d, t] = etx_from_rssi(RSSI_h[d][t])
        else:
            etx_h[d, t] = alpha_M * etx_h[d, t-1] + (1 - alpha_M) * (TX_h[d][t] / ACK_h[d][t])

M_v = np.cumsum(etx_h, axis=0)
plt.figure(figsize=(14, 6))
for d in range(depth):
    plt.plot(M_v[d], label=f'Hop {d+1}')
for b in range(1, segments_cnt):
    plt.axvline(b * seg_len, color='grey', linestyle='--', linewidth=1)

plt.title(rf'Cumulative metric $M_v(t)$ over depth-{depth} chain (α={alpha_M})')
plt.xlabel('Window index $t$')
plt.ylabel(r'$M_v(t)$')
plt.grid(True, linestyle='--', linewidth=0.5)
plt.legend(title='Hop', ncol=2, loc='upper left', bbox_to_anchor=(1,1))
plt.tight_layout()
plt.savefig('cumulative_metric.png', dpi=300)



