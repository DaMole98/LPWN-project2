import matplotlib.pyplot as plt
import numpy as np

# Define RSSI thresholds
RSSI_HIGH_REF = -45
RSSI_THR = -85

def default_etx_from_rssi(rssi):
    if rssi > RSSI_HIGH_REF:
        return 1.0
    if rssi < RSSI_THR:
        return 10.0
    span = RSSI_HIGH_REF - RSSI_THR
    offset = RSSI_HIGH_REF - rssi
    frac = offset / span
    return 1.0 + frac * 9.0

# Generate RSSI values and corresponding ETX values
rssi_values = np.linspace(-100, -30, 1000)
etx_values = [default_etx_from_rssi(rssi) for rssi in rssi_values]

# Plot
plt.figure(figsize=(10, 6))
plt.plot(rssi_values, etx_values, label="ETX(r)", color='blue')

# Add vertical dashed lines at threshold values
plt.axvline(RSSI_HIGH_REF, color='red', linestyle='--', label=r'$r_{high}$')
plt.axvline(RSSI_THR, color='green', linestyle='--', label=r'$r_{low}$')

# Labels and legend
plt.title("ETX Estimation from RSSI")
plt.xlabel("RSSI [dBm]")
plt.ylabel("ETX")
plt.grid(True)
plt.legend()
plt.tight_layout()

plt.savefig("default_etx.png")
