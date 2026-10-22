#!/bin/bash
# build.sh <path to open-gpu-kernel-modules 610.57.04 checkout>: builds nvclk with the Command Line Tools.
# NullMoth 2.4.3 is built on that tag (commit e4a5faa2); the RM SDK headers must match the driver.
set -euo pipefail
S=${1:?path to open-gpu-kernel-modules (tag 610.57.04)}/src
cd "$(dirname "$0")"
clang -O2 -Wall -o nvclk nvclk.c -I"$S/common/sdk/nvidia/inc" -I"$S/nvidia/arch/nvalloc/unix/include" -I"$S/common/inc" \
  -framework IOKit -framework CoreFoundation
echo "OK: $(pwd)/nvclk"
