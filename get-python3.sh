#!/bin/bash
set -e  # Exit immediately if any command fails

# --------------------------------------------------------
# Script to prepare a Debian/Ubuntu system for scientific
# Python development (numpy, scipy, matplotlib, pandas).
# --------------------------------------------------------

# Use non-interactive frontend for apt
export DEBIAN_FRONTEND=noninteractive

# 1) Update package lists to get the latest version info
echo "=== Updating package list..."
sudo apt update

# 2) Install core build tools and Python headers
echo "=== Installing build-essential, Python headers, and compilers..."
sudo apt install -y build-essential python3-dev python3-pip gfortran

# 3) Install BLAS/LAPACK libraries for fast linear algebra
echo "=== Installing BLAS/LAPACK libraries..."
sudo apt install -y libblas-dev liblapack-dev libatlas-base-dev

# 4) Install image- and compression-related headers (for Pillow, matplotlib)
echo "=== Installing image and compression libraries..."
sudo apt install -y zlib1g-dev libjpeg-dev libfreetype6-dev libpng-dev                  


# 5) Upgrade pip, setuptools, wheel to latest
echo "=== Upgrading pip, setuptools, and wheel..."
python3 -m pip install --upgrade pip setuptools wheel

# 6) Install the core scientific Python packages
echo "=== Installing numpy, scipy, matplotlib, pandas..."
python3 -m pip install numpy scipy matplotlib pandas

# 7) Final success message
echo "=== Installation complete!"
echo "You can now import numpy, scipy, matplotlib, and pandas in Python 3."
