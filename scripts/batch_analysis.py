"""
Script to analyze the simulation report file "BATCH-simulation-report-test_nogui_dc.txt".

This script extracts for each simulation run:
  - Overall Packet Delivery Ratio (PDR, in %)
  - Average Latency (in ms)
  - Overall Duty Cycle (in %)

It calculates for each metric:
  - Number of samples
  - Mean, Median, Sample Standard Deviation
  - 95% Confidence Interval on the mean

Finally, the script generates boxplots for each metric.
"""

import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
import scipy.stats as st
import math

def parse_report(file_path):
    """
    Parse the simulation report file and extract overall metrics for each simulation run:
      - Overall PDR (percentage)
      - Average Latency (in ms)
      - Overall Duty Cycle (percentage)

    Returns:
        pdr_list (list of float): List containing overall PDR values.
        latency_list (list of float): List containing average latency values.
        duty_cycle_list (list of float): List containing overall duty cycle values.
    """
    pdr_list = []
    latency_list = []
    duty_cycle_list = []

    # Regular expressions to extract values from the report lines
    re_pdr = re.compile(r"Overall PDR:\s*([0-9.]+)%")
    re_latency = re.compile(r"Average:\s*([0-9.]+)\s*ms")
    re_duty = re.compile(r"Overall Duty Cycle:\s*([0-9.]+)%")

    with open(file_path, "r") as f:
        for line in f:
            line = line.strip()
            # Extract Overall PDR
            match = re_pdr.search(line)
            if match:
                try:
                    pdr_list.append(float(match.group(1)))
                except ValueError:
                    pass
            # Extract Average Latency
            match = re_latency.search(line)
            if match:
                try:
                    latency_list.append(float(match.group(1)))
                except ValueError:
                    pass
            # Extract Overall Duty Cycle
            match = re_duty.search(line)
            if match:
                try:
                    duty_cycle_list.append(float(match.group(1)))
                except ValueError:
                    pass

    return pdr_list, latency_list, duty_cycle_list

def compute_statistics(data):
    """
    Compute statistics for the provided data.

    Args:
        data (list or array of float): The list of values.

    Returns:
        dict: A dictionary containing:
              - n: number of samples
              - mean: mean value
              - median: median value
              - std: sample standard deviation
              - ci: 95% confidence interval on the mean (tuple: (low, high))
    """
    n = len(data)
    if n == 0:
        return None

    mean_val = np.mean(data)
    median_val = np.median(data)
    std_val = np.std(data, ddof=1)  # sample standard deviation

    # Calculate the 95% confidence interval using the t-distribution
    t_crit = st.t.ppf(0.975, n - 1)
    margin = t_crit * std_val / math.sqrt(n)
    ci_low = mean_val - margin
    ci_high = mean_val + margin

    return {"n": n,
            "mean": mean_val,
            "median": median_val,
            "std": std_val,
            "ci": (ci_low, ci_high)}

def plot_boxplots(metric_data, metric_name, ylabel):
    """
    Create and display a boxplot for the given metric data.

    Args:
        metric_data (list or array of float): Data to be plotted.
        metric_name (str): Name of the metric (used in the title).
        ylabel (str): Label for the y-axis.
    """
    plt.figure(figsize=(8, 6))
    bp = plt.boxplot(metric_data, vert=True, patch_artist=True, widths=0.6)
    plt.title(f"Boxplot of {metric_name}")
    plt.ylabel(ylabel)
    plt.grid(True, linestyle='--', alpha=0.7)
    # Customize the appearance of the boxes
    for box in bp['boxes']:
        box.set(facecolor='#99ccff')
    #plt.show()
    plt.savefig(f"{metric_name}-boxplot.png")

def main():
    parser = argparse.ArgumentParser(
        description="Analyze a simulation report file and produce statistics & boxplots for PDR, Latency, and Duty Cycle.")
    parser.add_argument("file", type=str,
                        help="Path to the simulation report file (e.g., BATCH-simulation-report-test_nogui_dc.txt)")
    args = parser.parse_args()

    file_path = args.file
    print("\nParsing report file...")
    pdr_list, latency_list, duty_cycle_list = parse_report(file_path)

    if not (pdr_list and latency_list and duty_cycle_list):
        print("Error: Not all required data could be extracted. Please check the report file format.")
        return

    # Compute statistics for each metric
    stats_pdr = compute_statistics(pdr_list)
    stats_latency = compute_statistics(latency_list)
    stats_duty = compute_statistics(duty_cycle_list)

    separator = "=" * 50

    # Print formatted statistics for PDR
    print("\n" + separator)
    print("PDR Statistics (%)")
    print(separator)
    print(f"Number of Samples        : {stats_pdr['n']}")
    print(f"Mean PDR                 : {stats_pdr['mean']:.2f}%")
    print(f"Median PDR               : {stats_pdr['median']:.2f}%")
    print(f"Sample Standard Deviation: {stats_pdr['std']:.2f}")
    print(f"95% Confidence Interval  : ({stats_pdr['ci'][0]:.2f}%, {stats_pdr['ci'][1]:.2f}%)")

    # Print formatted statistics for Latency
    print("\n" + separator)
    print("Latency Statistics (ms)")
    print(separator)
    print(f"Number of Samples        : {stats_latency['n']}")
    print(f"Mean Latency             : {stats_latency['mean']:.2f} ms")
    print(f"Median Latency           : {stats_latency['median']:.2f} ms")
    print(f"Sample Standard Deviation: {stats_latency['std']:.2f} ms")
    print(f"95% Confidence Interval  : ({stats_latency['ci'][0]:.2f} ms, {stats_latency['ci'][1]:.2f} ms)")

    # Print formatted statistics for Duty Cycle
    print("\n" + separator)
    print("Duty Cycle Statistics (%)")
    print(separator)
    print(f"Number of Samples        : {stats_duty['n']}")
    print(f"Mean Duty Cycle          : {stats_duty['mean']:.2f}%")
    print(f"Median Duty Cycle        : {stats_duty['median']:.2f}%")
    print(f"Sample Standard Deviation: {stats_duty['std']:.2f}")
    print(f"95% Confidence Interval  : ({stats_duty['ci'][0]:.2f}%, {stats_duty['ci'][1]:.2f}%)")
    print(separator + "\n")

    # Generate and display boxplots for each metric
    print("Displaying boxplot for PDR...")
    plot_boxplots(pdr_list, "PDR", "PDR (%)")

    print("Displaying boxplot for Latency...")
    plot_boxplots(latency_list, "Latency", "Latency (ms)")

    print("Displaying boxplot for Duty Cycle...")
    plot_boxplots(duty_cycle_list, "Duty Cycle", "Duty Cycle (%)")

if __name__ == "__main__":
    main()
