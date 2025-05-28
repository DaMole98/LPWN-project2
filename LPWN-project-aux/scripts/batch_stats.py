#!/usr/bin/env python3

import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
import scipy.stats as st
import math
import os


def parse_report(file_path):
    pdr_list, latency_list, duty_list = [], [], []
    re_pdr     = re.compile(r"Overall PDR:\s*([0-9.]+)%")
    re_lat     = re.compile(r"Average:\s*([0-9.]+)\s*ms")
    re_duty    = re.compile(r"Overall Duty Cycle:\s*([0-9.]+)%")
    with open(file_path) as f:
        for line in f:
            # Search for PDR
            m = re_pdr.search(line)
            if m:
                pdr_list.append(float(m.group(1)))
            # Search for latency
            m = re_lat.search(line)
            if m:
                latency_list.append(float(m.group(1)))
            # Search for duty cycle
            m = re_duty.search(line)
            if m:
                duty_list.append(float(m.group(1)))

    return pdr_list, latency_list, duty_list

def remove_outliers(data, metric):
    """
    Remove the worst outliers using 1.5*IQR:
      - for PDR, remove values < Q1 - 1.5*IQR
      - for Latency and Duty Cycle, remove values > Q3 + 1.5*IQR
    """
    if not data:
        return data
    arr = np.array(data)
    q1, q3 = np.percentile(arr, [25, 75])
    iqr = q3 - q1
    if metric == 'PDR':
        lower_bound = q1 - 1.5 * iqr
        filtered = arr[arr >= lower_bound]
    else:
        upper_bound = q3 + 1.5 * iqr
        filtered = arr[arr <= upper_bound]
    return filtered.tolist()

def compute_statistics(data):
    n = len(data)
    if n == 0:
        return {"n": 0, "mean": math.nan, "median": math.nan, "std": math.nan, "ci": (math.nan, math.nan)}
    mean = np.mean(data)
    median = np.median(data)
    std = np.std(data, ddof=1)
    t_value = st.t.ppf(0.975, n-1)
    margin = t_value * std / math.sqrt(n)
    return {"n": n, "mean": mean, "median": median, "std": std, "ci": (mean - margin, mean + margin)}

def main():
    parser = argparse.ArgumentParser(
        description="Compare simulation batches with outlier removal.")
    parser.add_argument('files', nargs='+', help="List of report files to process")
    parser.add_argument('-o', '--output', default='summary.txt', help="Output summary filename")
    args = parser.parse_args()

    # Derive base names and labels
    filenames = [os.path.splitext(os.path.basename(f))[0] for f in args.files]
    labels = [name.lstrip("batch-") for name in filenames]

    # Parse and collect metrics
    metrics = {'PDR': [], 'Latency': [], 'Duty Cycle': []}
    for path in args.files:
        pdr_vals, lat_vals, duty_vals = parse_report(path)
        metrics['PDR'].append(pdr_vals)
        metrics['Latency'].append(lat_vals)
        metrics['Duty Cycle'].append(duty_vals)

    # Remove the worst outliers
    for metric_name in metrics:
        cleaned = []
        for series in metrics[metric_name]:
            cleaned.append(remove_outliers(series, metric_name))
        metrics[metric_name] = cleaned

    # Prepare duty-cycle data, skip NullRDC entries
    dc_data = [
        [] if 'nrdc' in fname.lower() else duty_series
        for fname, duty_series in zip(filenames, metrics['Duty Cycle'])
    ]

    # Compute statistics per batch
    stats = []
    for fname, pdr_vals, lat_vals, duty_vals in zip(filenames,
                                                    metrics['PDR'],
                                                    metrics['Latency'],
                                                    metrics['Duty Cycle']):
        stats.append({
            'file': fname,
            'PDR': compute_statistics(pdr_vals),
            'Latency': compute_statistics(lat_vals),
            'Duty Cycle': compute_statistics(duty_vals)
        })

    # Create boxplots
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    axes[0].boxplot(metrics['PDR'], notch=True, widths=0.6)
    axes[0].set_title("PDR Comparison")
    axes[0].set_ylabel("PDR (%)")
    axes[0].set_xticks(range(1, len(labels) + 1))
    axes[0].set_xticklabels(labels, rotation=45, ha='right')
    axes[0].grid(True, linestyle='--', alpha=0.7)

    axes[1].boxplot(metrics['Latency'], notch=True, widths=0.6)
    axes[1].set_title("Latency Comparison")
    axes[1].set_ylabel("Latency (ms)")
    axes[1].set_xticks(range(1, len(labels) + 1))
    axes[1].set_xticklabels(labels, rotation=45, ha='right')
    axes[1].grid(True, linestyle='--', alpha=0.7)

    axes[2].boxplot(dc_data, notch=True, widths=0.6)
    axes[2].set_title("Duty Cycle Comparison")
    axes[2].set_ylabel("Duty Cycle (%)")
    axes[2].set_xticks(range(1, len(labels) + 1))
    axes[2].set_xticklabels(labels, rotation=45, ha='right')
    axes[2].grid(True, linestyle='--', alpha=0.7)

    plt.tight_layout()
    plt.savefig("comparison_boxplots.png")
    plt.close(fig)

    # Write summary to file
    separator = '=' * 60 + '\n'
    with open(args.output, 'w') as out_file:
        for entry in stats:
            out_file.write(f"{separator}{entry['file']} Summary{separator}")
            sp = entry['PDR']
            out_file.write(
                f"PDR (%, n={sp['n']}): mean={sp['mean']:.2f}%, "
                f"median={sp['median']:.2f}%, std={sp['std']:.2f}, "
                f"CI=({sp['ci'][0]:.2f}%, {sp['ci'][1]:.2f}%)\n"
            )
            sl = entry['Latency']
            out_file.write(
                f"Latency (ms, n={sl['n']}): mean={sl['mean']:.2f} ms, "
                f"median={sl['median']:.2f} ms, std={sl['std']:.2f} ms, "
                f"CI=({sl['ci'][0]:.2f} ms, {sl['ci'][1]:.2f} ms)\n"
            )
            sd = entry['Duty Cycle']
            out_file.write(
                f"Duty Cycle (%, n={sd['n']}): mean={sd['mean']:.2f}%, "
                f"median={sd['median']:.2f}%, std={sd['std']:.2f}%, "
                f"CI=({sd['ci'][0]:.2f}%, {sd['ci'][1]:.2f}%)\n\n"
            )

    print(f"Summary written to {args.output}")
    print("Boxplot saved as comparison_boxplots.png")

if __name__ == '__main__':
    main()
