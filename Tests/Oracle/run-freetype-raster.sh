#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: Tests/Oracle/run-freetype-raster.sh /path/to/freetype-2.14.3" >&2
    exit 2
fi

oracle_freetype_source=$1
oracle_package_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
oracle_build_dir=$(mktemp -d /private/tmp/silex-font-oracle.XXXXXX)
trap 'rm -rf "$oracle_build_dir"' EXIT HUP INT TERM

cc -std=c11 -O2 -g0 \
    -I"$oracle_freetype_source/include" \
    "$oracle_package_dir/Tests/Oracle/FreeTypeRaster.c" \
    "$oracle_package_dir/Boundary/macos-arm64/libfreetype.a" \
    -o "$oracle_build_dir/freetype-raster"

"$oracle_build_dir/freetype-raster" "$oracle_package_dir/Tests/Fixtures/Minimal.ttf"
