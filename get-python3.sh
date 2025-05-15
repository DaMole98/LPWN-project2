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
sudo apt-get update

# 2) Install core build tools and Python headers
echo "=== Installing build-essential, Python headers, and compilers..."
sudo apt-get install -y \
    build-essential \           # GCC, make, etc.
    python3-dev \               # Python C headers
    python3-pip \               # pip for Python 3
    gfortran \                  # Fortran compiler (needed by SciPy)

# 3) Install BLAS/LAPACK libraries for fast linear algebra
echo "=== Installing BLAS/LAPACK libraries..."
sudo apt-get install -y \
    libblas-dev \
    liblapack-dev \
    libatlas-base-dev

# 4) Install image- and compression-related headers (for Pillow, matplotlib)
echo "=== Installing image and compression libraries..."
sudo apt-get install -y \
    zlib1g-dev \                # zlib (PNG, TIFF, etc.)
    libjpeg-dev \               # JPEG support
    libfreetype6-dev \          # FreeType (fonts)
    libpng-dev                  # PNG library


# 5) Upgrade pip, setuptools, wheel to latest
echo "=== Upgrading pip, setuptools, and wheel..."
python3 -m pip install --upgrade pip setuptools wheel

# 6) Install the core scientific Python packages
echo "=== Installing numpy, scipy, matplotlib, pandas..."
python3 -m pip install numpy scipy matplotlib pandas

# 7) Final success message
echo "=== Installation complete!"
echo "You can now import numpy, scipy, matplotlib, and pandas in Python 3."
