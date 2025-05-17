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
            if m := re_pdr.search(line):
                pdr_list.append(float(m.group(1)))
            if m := re_lat.search(line):
                latency_list.append(float(m.group(1)))
            if m := re_duty.search(line):
                duty_list.append(float(m.group(1)))
    return pdr_list, latency_list, duty_list

def compute_statistics(data):
    n = len(data)
    if n == 0:
        return {"n":0,"mean":math.nan,"median":math.nan,"std":math.nan,"ci":(math.nan,math.nan)}
    mean = np.mean(data)
    med  = np.median(data)
    std  = np.std(data, ddof=1)
    t   = st.t.ppf(0.975, n-1)
    m   = t * std / math.sqrt(n)
    return {"n":n,"mean":mean,"median":med,"std":std,"ci":(mean-m, mean+m)}

def main():
    p = argparse.ArgumentParser(
        description="Compare simulation batches with side-by-side boxplots.")
    p.add_argument('files', nargs='+')
    p.add_argument('-o','--output', default='summary.txt')
    args = p.parse_args()

    # base names and labels
    names  = [os.path.splitext(os.path.basename(f))[0] for f in args.files]
    labels = [name.lstrip("batch-") for name in names]

    # parse and store
    metrics = {'PDR':[], 'Latency':[], 'Duty Cycle':[]}
    stats   = []
    for fpath in args.files:
        pdr, lat, duty = parse_report(fpath)
        metrics['PDR'].append(pdr)
        metrics['Latency'].append(lat)
        metrics['Duty Cycle'].append(duty)
        stats.append({
            'file':os.path.basename(fpath),
            'PDR':compute_statistics(pdr),
            'Latency':compute_statistics(lat),
            'Duty Cycle':compute_statistics(duty)
        })

    # prepare DC data: replace nullrdc batch with empty
    dc_data = [
        [] if 'nrdc' in nm.lower() else duty
        for nm, duty in zip(names, metrics['Duty Cycle'])
    ]

    # side-by-side
    fig, axs = plt.subplots(1, 3, figsize=(15, 5))

    # PDR
    axs[0].boxplot(metrics['PDR'], notch=True, widths=0.6)
    axs[0].set_title("PDR Comparison")
    axs[0].set_ylabel("PDR(%)")
    axs[0].set_xticks(range(1, len(labels)+1))
    axs[0].set_xticklabels(labels, rotation=45, ha='right')
    axs[0].grid(True, linestyle='--', alpha=0.7)

    # Latency
    axs[1].boxplot(metrics['Latency'], notch=True, widths=0.6)
    axs[1].set_title("Latency Comparison")
    axs[1].set_ylabel("ms")
    axs[1].set_xticks(range(1, len(labels)+1))
    axs[1].set_xticklabels(labels, rotation=45, ha='right')
    axs[1].grid(True, linestyle='--', alpha=0.7)

    # Duty Cycle
    axs[2].boxplot(dc_data, notch=True, widths=0.6)
    axs[2].set_title("Duty Cycle Comparison")
    axs[2].set_ylabel("DC(%)")
    axs[2].set_xticks(range(1, len(labels)+1))
    axs[2].set_xticklabels(labels, rotation=45, ha='right')
    axs[2].grid(True, linestyle='--', alpha=0.7)

    plt.tight_layout()
    plt.savefig("comparison_boxplots.png")
    plt.close(fig)

    # write summary
    sep = '='*60 + '\n'
    with open(args.output, 'w') as out:
        for e in stats:
            out.write(f"{sep}{e['file']} Summary{sep}")
            sp = e['PDR']
            out.write(f"PDR (%, n={sp['n']}): mean={sp['mean']:.2f}%, "
                      f"median={sp['median']:.2f}%, std={sp['std']:.2f}, "
                      f"ci=({sp['ci'][0]:.2f}%,{sp['ci'][1]:.2f}%)\n")
            sl = e['Latency']
            out.write(f"Latency (ms, n={sl['n']}): mean={sl['mean']:.2f}ms, "
                      f"median={sl['median']:.2f}ms, std={sl['std']:.2f}ms, "
                      f"ci=({sl['ci'][0]:.2f}ms,{sl['ci'][1]:.2f}ms)\n")
            sd = e['Duty Cycle']
            out.write(f"Duty Cycle (%, n={sd['n']}): mean={sd['mean']:.2f}%, "
                      f"median={sd['median']:.2f}%, std={sd['std']:.2f}%, "
                      f"ci=({sd['ci'][0]:.2f}%,{sd['ci'][1]:.2f}%)\n\n")

    print("Summary written to", args.output)
    print("Boxplot saved as comparison_boxplots.png")

if __name__ == '__main__':
    main()

