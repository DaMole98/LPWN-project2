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

def process_group(dirs, base_dir, summary_path, plot_path, title):
    """
    Given a list of subdirectory names (dirs) all belonging to the same testbed,
    parse stats, write a summary file and produce a side-by-side bar plot.
    """
    labels, raw_names = [], []
    pdr_vals, dc_vals = [], []
    summary_lines = []

    # parse each folder in this testbed
    for d in dirs:
        stats_txt = os.path.join(base_dir, d, "stats.txt")
        stats_dc  = os.path.join(base_dir, d, "stats-DC.txt")
        if not (os.path.isfile(stats_txt) and os.path.isfile(stats_dc)):
            continue

        pdr_line, pdr_v = parse_overall_pdr(stats_txt)
        dc_line,  dc_v  = parse_average_dc(stats_dc)

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

    # write summary file
    with open(summary_path, 'w') as f:
        f.write("\n".join(summary_lines))
    print(f"✔ Summary written to {summary_path}")

    # prepare the bar chart
    n = len(labels)
    x = np.arange(n)
    cmap = plt.get_cmap('tab20', n)
    colors = [cmap(i) for i in range(n)]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # --- Average PDR ---
    bars1 = ax1.bar(x, pdr_vals, color=colors, width=0.6)
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels, rotation=45, ha='right')
    ax1.set_ylabel("PDR (%)")
    ax1.set_title("Average PDR")
    ax1.set_ylim(0, 100)
    ax1.grid(True, linestyle='--', alpha=0.4)
    for bar, v in zip(bars1, pdr_vals):
        ax1.text(bar.get_x() + bar.get_width()/2, v + 1,
                 f"{v:.1f}%", ha='center', va='bottom')

    # --- Average Duty Cycle (skip nullrdc) ---
    for i, v in enumerate(dc_vals):
        if "nullrdc" not in raw_names[i]:
            bar = ax2.bar(i, v, color=colors[i], width=0.6)[0]
            ax2.text(bar.get_x() + bar.get_width()/2, v + 0.1,
                     f"{v:.2f}%", ha='center', va='bottom')

    # build labels list: empty for nullrdc entries
    dc_labels = [
        lbl if "nullrdc" not in raw_names[i] else ""
        for i, lbl in enumerate(labels)
    ]
    ax2.set_xticks(x)
    ax2.set_xticklabels(dc_labels, rotation=45, ha='right')
    ax2.set_ylabel("DC (%)")
    ax2.set_title("Average Duty Cycle")
    valid = [dc_vals[i] for i in range(n) if "nullrdc" not in raw_names[i]]
    if valid:
        ax2.set_ylim(0, min(max(valid)*1.1, 100))
    ax2.grid(True, linestyle='--', alpha=0.4)

    # super-title and layout
    fig.suptitle(title, fontsize=16)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.savefig(plot_path)
    plt.close(fig)
    print(f"✔ Plot saved as      {plot_path}")

def main(base_dir, summary_file, plot_file, title):
    # discover all subdirectories
    subs = sorted(d for d in os.listdir(base_dir)
                  if os.path.isdir(os.path.join(base_dir, d)))

    # group by the 'nodes-X-Y' testbed tag
    groups = {}
    for d in subs:
        m = re.search(r'nodes-(\d+-\d+)', d, flags=re.IGNORECASE)
        if m:
            tag = m.group(1)
            groups.setdefault(tag, []).append(d)

    if not groups:
        print("No 'nodes-<X>-<Y>' groups found. Nothing to do.")
        return

    # process each testbed group separately
    for tag, dirs in sorted(groups.items()):
        # build per-group filenames
        base_s, ext_s = os.path.splitext(summary_file)
        base_p, ext_p = os.path.splitext(plot_file)
        summary_path = f"{base_s}_{tag}{ext_s}"
        plot_path    = f"{base_p}_{tag}{ext_p}"

        group_title = f"{title} (nodes-{tag})" if title else f"nodes-{tag}"
        process_group(dirs, base_dir, summary_path, plot_path, group_title)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Extract Overall PDR and Average DC, transform labels, "
                    "split by testbed (nodes-X-Y), and generate per-testbed summaries + plots."
    )
    parser.add_argument("base_dir",
                        help="Parent directory of all test subfolders")
    parser.add_argument("-s", "--summary",
                        default="summary.txt",
                        help="Base name for summary files (will append '_<X-Y>')")
    parser.add_argument("-p", "--plot",
                        default="pdr_dc_plot.png",
                        help="Base name for plot files (will append '_<X-Y>')")
    parser.add_argument("-t", "--title",
                        default="PDR & DC Comparison",
                        help="Base title for the figures")
    args = parser.parse_args()
    main(args.base_dir, args.summary, args.plot, args.title)
