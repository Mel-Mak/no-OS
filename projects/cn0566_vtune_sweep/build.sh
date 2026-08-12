#!/bin/bash
# Build cn0566_vtune_sweep directly with gcc on Raspberry Pi.
# Usage: cd no-OS/projects/cn0566_vtune_sweep && bash build.sh

set -e

NO_OS="$(cd ../../ && pwd)"
DRIVERS="$NO_OS/drivers"
PLATFORM="$NO_OS/drivers/platform/linux"
INCLUDE="$NO_OS/include"
UTIL="$NO_OS/util"
API="$NO_OS/drivers/api"
OUT="build"

mkdir -p "$OUT"

echo "Building cn0566_vtune_sweep..."

gcc -g3 -DLINUX_PLATFORM \
    -I"$INCLUDE" \
    -I"$API" \
    -I"src/" \
    -I"$DRIVERS/adc/ad7291" \
    -I"$DRIVERS/frequency/adf4159" \
    -I"$PLATFORM" \
    \
    src/main.c \
    "$DRIVERS/adc/ad7291/ad7291.c" \
    "$DRIVERS/frequency/adf4159/adf4159.c" \
    "$API/no_os_i2c.c" \
    "$API/no_os_spi.c" \
    "$API/no_os_gpio.c" \
    "$PLATFORM/linux_i2c.c" \
    "$PLATFORM/linux_spi.c" \
    "$PLATFORM/linux_gpio.c" \
    "$PLATFORM/linux_delay.c" \
    "$UTIL/no_os_alloc.c" \
    "$UTIL/no_os_util.c" \
    "$UTIL/no_os_mutex.c" \
    -lm -lpthread \
    -o "$OUT/cn0566_vtune_sweep"

echo "Build succeeded: $OUT/cn0566_vtune_sweep"
echo "Run with: sudo ./$OUT/cn0566_vtune_sweep"
