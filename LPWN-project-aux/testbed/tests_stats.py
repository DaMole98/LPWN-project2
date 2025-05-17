#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import argparse
import numpy as np
import matplotlib.pyplot as plt

def transform_label(dirname):
    """
    Trim the leading date-time prefix and trailing duration suffix,
    then replace:
      - 'contikimacNN' → 'CMNN'
      - 'nullrdcNN?' → 'NRDCNN?'
      - 'nodes' → 'N'
    """
    name = re.sub(r'^\d{2}-\d{2}_\d{4}_', '', dirname)
    name = re.sub(r'_\d+s$', '', name)
    name = re.sub(r'contikimac(\d+)', r'CM\1', name, flags=re.IGNORECASE)
    name = re.sub(r'nullrdc(\d*)', r'NRDC\1', name, flags=re.IGNORECASE)
    name = re.sub(r'nodes', 'N', name, flags=re.IGNORECASE)
    return name

def parse_overall_pdr(path):
    """Extract 'Overall PDR: xx.xx%' and return (raw_line, float_value)."""
    with open(path) as f:
        for L in f:
            if L.startswith("Overall PDR:"):
                m = re.search(r"Overall PDR:\s*([\d\.]+)%", L)
                if m:
                    return L.strip(), float(m.group(1))
    raise RuntimeError(f"Cannot find Overall PDR in {path}")

def parse_average_dc(path):
    """Extract 'Average Duty Cycle: xx.xx%' and return (raw_line, float_value)."""
    with open(path) as f:
        for L in f:
            if L.startswith("Average Duty Cycle:"):
                m = re.search(r"Average Duty Cycle:\s*([\d\.]+)%", L)
                if m:
                    return L.strip(), float(m.group(1))
    raise RuntimeError(f"Cannot find Average Duty Cycle in {path}")

def main(base_dir, summary_file, plot_file, title):
    # discover subdirectories
    subs = sorted(d for d in os.listdir(base_dir)
                  if os.path.isdir(os.path.join(base_dir, d)))

    labels, raw_names = [], []
    pdr_vals, dc_vals = [], []
    summary_lines = []

    # parse each folder
    for d in subs:
        p_path  = os.path.join(base_dir, d, "stats.txt")
        dc_path = os.path.join(base_dir, d, "stats-DC.txt")
        if not (os.path.isfile(p_path) and os.path.isfile(dc_path)):
            continue

        pdr_line, pdr_v = parse_overall_pdr(p_path)
        dc_line,  dc_v  = parse_average_dc(dc_path)

        labels.append(transform_label(d))
        raw_names.append(d.lower())
        pdr_vals.append(pdr_v)
        dc_vals.append(dc_v)

        summary_lines += [
            "="*60,
            f"{d} Summary",
            "="*60,
            pdr_line,
            dc_line,
            ""
        ]

    # write summary.txt
    with open(summary_file, 'w') as f:
        f.write("\n".join(summary_lines))

    n = len(labels)
    x = np.arange(n)

    # pick n distinct colors from tab20
    cmap = plt.get_cmap('tab20', n)
    colors = [cmap(i) for i in range(n)]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))


    # --- Average PDR bars with per-folder colors + labels ---
    bars1 = ax1.bar(x, pdr_vals, color=colors, width=0.6)
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, rotation=45, ha='right')
    ax1.set_ylabel("PDR (%)")
    ax1.set_title("Average PDR")
    ax1.set_ylim(0, 100)
    ax1.grid(True, linestyle='--', alpha=0.4)
    # annotate PDR values
    for bar, v in zip(bars1, pdr_vals):
        ax1.text(bar.get_x() + bar.get_width()/2, v + 1,
                 f"{v:.1f}%", ha='center', va='bottom')

    # --- Average DC bars with same per-folder colors + labels, skipping nullrdc ---
    for i, v in enumerate(dc_vals):
        if "nullrdc" not in raw_names[i]:
            bar = ax2.bar(i, v, color=colors[i], width=0.6)[0]
            # annotate DC value
            ax2.text(bar.get_x() + bar.get_width()/2, v + 0.1,
                     f"{v:.2f}%", ha='center', va='bottom')

    ax2.set_xticks(x)
    ax2.set_xticklabels(labels, rotation=45, ha='right')
    ax2.set_ylabel("DC (%)")
    ax2.set_title("Average Duty Cycle")
    valid = [dc_vals[i] for i in range(n) if "nullrdc" not in raw_names[i]]
    if valid:
        ax2.set_ylim(0, min(max(valid)*1.1, 100))
    ax2.grid(True, linestyle='--', alpha=0.4)

    fig.suptitle(title)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
   # plt.tight_layout()
    plt.savefig(plot_file)

    print(f"✔ Summary written to {summary_file}")
    print(f"✔ Plot saved as      {plot_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Extract Overall PDR and Average DC, transform labels, "
                    "assign distinct colors per folder, annotate values, and generate summary + plots."
    )
    parser.add_argument("base_dir",
                        help="Parent directory of all test subfolders")
    parser.add_argument("-s", "--summary",
                        default="summary.txt",
                        help="Output summary filename (default: summary.txt)")
    parser.add_argument("-p", "--plot",
                        default="pdr_dc_plot.png",
                        help="Output plot filename (default: pdr_dc_plot.png)")
    parser.add_argument("-t", "--title",
                        help="Title of the plot")

    args = parser.parse_args()
    main(args.base_dir, args.summary, args.plot, args.title)


