# Tree-Based Any-to-Any Routing Project (2024-2025)

## Overview

This project implements a tree-based any-to-any routing protocol atop an existing collection tree in low-power wireless networks using Contiki OS. It supports various communication patterns: upward (to the sink), downward (to children), direct neighbor communication, and multi-hop any-to-any routing via common ancestors.

## Project Structure

```
project-root/
├── src/                 # Source files
│   ├── rp.c
│   ├── metric.c
│   └── nbr_tbl_utils.c
├── include/             # Header files
│   ├── rp.h
│   ├── metric.h
│   └── nbr_tbl_utils.h
├── scripts/             # Analysis and simulation scripts
│   ├── analysis.py
│   ├── energest-stats.py
│   ├── batch_runner.py
│   ├── batch_analysis.py
│   ├── metric_plots.py
│   ├── parser.py
│   ├── get-pip.py
│   └── etx_estimation_plot.py
├── Makefile             # Compilation instructions
└── project-conf.h       # Project configuration
```

## Compilation & Execution

Compile using the provided Makefile:

```bash
make TARGET=sky
```

### Simulation (Cooja)

Run simulations in Cooja using `batch_runner.py`, which automates compiling and executing multiple simulation runs:

```bash
cd scripts
python batch_runner.py
```

### Testbed Experiments

Optional: Run on Zolertia Firefly nodes. Ensure node IDs and settings match the testbed.

## RDC Configuration

Edit the `project-conf.h` to switch between NullRDC and ContikiMAC:

```c
// For NullRDC
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC nullrdc_driver
#define RDC_MODE RDC_NULLRDC

// For ContikiMAC
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC contikimac_driver
#define RDC_MODE RDC_CONTIKIMAC
```

Comment/uncomment these sections accordingly.

## Python Scripts (in scripts/)

* `analysis.py`: Parses logs (`recv.csv` and `sent.csv`) to compute Packet Delivery Rate (PDR), latency, and duty cycle.

```bash
python analysis.py <log_directory> --cooja|--testbed
```

* `energest-stats.py`: Analyzes energest logs to compute node and network-wide duty cycles.

```bash
python energest-stats.py <logfile> [-t|--testbed]
```

* `batch_analysis.py`: Aggregates and visualizes results from multiple runs (PDR, latency, DC).

```bash
python batch_analysis.py <simulation_report_file>
```

* `metric_plots.py` & `etx_estimation_plot.py`: Generate plots for ETX evolution and estimation based on RSSI data.

```bash
python metric_plots.py
python etx_estimation_plot.py
```

* `parser.py`: Converts raw `.log` files from simulations/testbed experiments into structured CSV files.

```bash
python parser.py <logfile> --cooja|--testbed
```

## Evaluation Metrics

* **Packet Delivery Rate (PDR)**
* **Duty Cycle (DC)**
* **End-to-End Latency** (Cooja only)

Use provided Python scripts for detailed evaluation.

## Notes

For detailed protocol description and further instructions, refer to the project report.
