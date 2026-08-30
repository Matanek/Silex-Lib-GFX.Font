#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: Native/build-font-boundary.sh /path/to/freetype-2.14.3 /path/to/harfbuzz-14.2.1" >&2
    exit 2
fi

font_native_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
font_package_dir=$(CDPATH= cd -- "$font_native_dir/.." && pwd)
font_freetype_source=$1
font_harfbuzz_source=$2
font_build_root=$(mktemp -d /private/tmp/silex-gfx-font.XXXXXX)
font_zig_cache="$font_build_root/zig-cache"
font_llvm_objcopy=${LLVM_OBJCOPY:-}
if [ -z "$font_llvm_objcopy" ]; then
    font_llvm_objcopy=$(command -v llvm-objcopy || true)
fi
if [ -z "$font_llvm_objcopy" ] && [ -x /opt/homebrew/opt/llvm/bin/llvm-objcopy ]; then
    font_llvm_objcopy=/opt/homebrew/opt/llvm/bin/llvm-objcopy
fi
if [ ! -x "$font_llvm_objcopy" ]; then
    echo "build requires llvm-objcopy; set LLVM_OBJCOPY to its path" >&2
    exit 2
fi
export SOURCE_DATE_EPOCH=0
export ZERO_AR_DATE=1
trap 'rm -rf "$font_build_root"' EXIT HUP INT TERM

repack_cross_archive() {
    font_repack_input=$1
    font_repack_output=$2
    font_repack_suffix=$3
    font_repack_label=$4
    font_repack_root="$font_build_root/repack-$font_repack_label"
    font_repack_raw="$font_repack_root/raw"
    font_repack_stripped="$font_repack_root/stripped"

    mkdir -p "$font_repack_raw" "$font_repack_stripped"
    (
        cd "$font_repack_raw"
        "$font_native_dir/Toolchains/zig-ar" x "$font_repack_input"
    )
    for font_repack_object in "$font_repack_raw"/*."$font_repack_suffix"; do
        font_repack_name=${font_repack_object##*/}
        "$font_llvm_objcopy" --strip-debug \
            "$font_repack_object" \
            "$font_repack_stripped/$font_repack_name"
    done
    rm -f "$font_repack_output"
    (
        cd "$font_repack_stripped"
        "$font_native_dir/Toolchains/zig-ar" rcsD \
            "$font_repack_output" \
            *."$font_repack_suffix"
    )
}

build_target() {
    font_name=$1
    font_system=$2
    font_processor=$3
    font_cc=$4
    font_cxx=$5
    font_extension=$6
    font_prefix=$7
    font_install_freetype="$font_build_root/install-freetype-$font_name"
    font_install_harfbuzz="$font_build_root/install-harfbuzz-$font_name"
    font_build_freetype="$font_build_root/build-freetype-$font_name"
    font_build_harfbuzz="$font_build_root/build-harfbuzz-$font_name"
    font_destination="$font_package_dir/Boundary/$font_name"
    font_shim_object_extension=o
    if [ "$font_system" = Windows ]; then
        font_shim_object_extension=obj
    fi

    env ZIG_GLOBAL_CACHE_DIR="$font_zig_cache" cmake \
        -S "$font_freetype_source" \
        -B "$font_build_freetype" \
        -DCMAKE_SYSTEM_NAME="$font_system" \
        -DCMAKE_SYSTEM_PROCESSOR="$font_processor" \
        -DCMAKE_C_COMPILER="$font_cc" \
        -DCMAKE_AR="$font_native_dir/Toolchains/zig-ar" \
        -DCMAKE_RANLIB="$font_native_dir/Toolchains/zig-ranlib" \
        -DCMAKE_RC_COMPILER="$font_native_dir/Toolchains/zig-rc" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_INSTALL_PREFIX="$font_install_freetype" \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_C_FLAGS_RELEASE=-O3 -g0 -DNDEBUG -ffile-prefix-map=$font_build_root=/silex-build -ffile-prefix-map=$font_freetype_source=/sources/freetype -ffile-prefix-map=$font_package_dir=/sources/gfx-font" \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_ZLIB=ON \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=ON \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON \
        -DFT_ENABLE_ERROR_STRINGS=ON
    env ZIG_GLOBAL_CACHE_DIR="$font_zig_cache" cmake --build "$font_build_freetype" --target install -j 8

    env ZIG_GLOBAL_CACHE_DIR="$font_zig_cache" cmake \
        -S "$font_harfbuzz_source" \
        -B "$font_build_harfbuzz" \
        -DCMAKE_SYSTEM_NAME="$font_system" \
        -DCMAKE_SYSTEM_PROCESSOR="$font_processor" \
        -DCMAKE_C_COMPILER="$font_cc" \
        -DCMAKE_CXX_COMPILER="$font_cxx" \
        -DCMAKE_AR="$font_native_dir/Toolchains/zig-ar" \
        -DCMAKE_RANLIB="$font_native_dir/Toolchains/zig-ranlib" \
        -DCMAKE_RC_COMPILER="$font_native_dir/Toolchains/zig-rc" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_INSTALL_PREFIX="$font_install_harfbuzz" \
        -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_C_FLAGS_RELEASE=-O3 -g0 -DNDEBUG -ffile-prefix-map=$font_build_root=/silex-build -ffile-prefix-map=$font_harfbuzz_source=/sources/harfbuzz -ffile-prefix-map=$font_package_dir=/sources/gfx-font" \
        "-DCMAKE_CXX_FLAGS_RELEASE=-O3 -g0 -DNDEBUG -DHB_NO_MMAP -DHB_NO_OPEN -ffile-prefix-map=$font_build_root=/silex-build -ffile-prefix-map=$font_harfbuzz_source=/sources/harfbuzz -ffile-prefix-map=$font_package_dir=/sources/gfx-font" \
        -DBUILD_SHARED_LIBS=OFF \
        -DHB_HAVE_CAIRO=OFF \
        -DHB_HAVE_FREETYPE=OFF \
        -DHB_HAVE_GRAPHITE2=OFF \
        -DHB_HAVE_GLIB=OFF \
        -DHB_HAVE_ICU=OFF \
        -DHB_HAVE_CORETEXT=OFF \
        -DHB_HAVE_UNISCRIBE=OFF \
        -DHB_HAVE_GDI=OFF \
        -DHB_HAVE_DIRECTWRITE=OFF \
        -DHB_BUILD_UTILS=OFF \
        -DHB_BUILD_SUBSET=OFF \
        -DHB_BUILD_RASTER=OFF \
        -DHB_BUILD_VECTOR=OFF \
        -DHB_BUILD_GPU=OFF
    env ZIG_GLOBAL_CACHE_DIR="$font_zig_cache" cmake --build "$font_build_harfbuzz" --target install -j 8

    mkdir -p "$font_destination"
    env ZIG_GLOBAL_CACHE_DIR="$font_zig_cache" "$font_cc" \
        -std=c11 -O3 -g0 -DNDEBUG \
        -ffile-prefix-map="$font_build_root"=/silex-build \
        -ffile-prefix-map="$font_package_dir"=/sources/gfx-font \
        -I"$font_install_freetype/include/freetype2" \
        -I"$font_install_harfbuzz/include/harfbuzz" \
        -I"$font_native_dir" \
        -c "$font_native_dir/SilexFont.c" \
        -o "$font_build_root/SilexFont-$font_name.$font_shim_object_extension"
    env ZIG_GLOBAL_CACHE_DIR="$font_zig_cache" "$font_cc" \
        -std=c11 -O3 -g0 -DNDEBUG \
        -ffile-prefix-map="$font_build_root"=/silex-build \
        -ffile-prefix-map="$font_package_dir"=/sources/gfx-font \
        -I"$font_native_dir" \
        -c "$font_native_dir/abi-layout.c" \
        -o "$font_build_root/abi-layout-$font_name.$font_shim_object_extension"

    if [ "$font_system" != Windows ]; then
        rm -f "$font_destination/${font_prefix}SilexFont.$font_extension"
        "$font_native_dir/Toolchains/zig-ar" rcsD \
            "$font_destination/${font_prefix}SilexFont.$font_extension" \
            "$font_build_root/SilexFont-$font_name.$font_shim_object_extension" \
            "$font_build_root/abi-layout-$font_name.$font_shim_object_extension"
        cp "$font_install_freetype/lib/libfreetype.a" \
            "$font_destination/${font_prefix}freetype.$font_extension"
        cp "$font_install_harfbuzz/lib/libharfbuzz.a" \
            "$font_destination/${font_prefix}harfbuzz.$font_extension"
    else
        font_shim_archive="$font_build_root/SilexFont-$font_name.a"
        "$font_native_dir/Toolchains/zig-ar" rcsD \
            "$font_shim_archive" \
            "$font_build_root/SilexFont-$font_name.$font_shim_object_extension" \
            "$font_build_root/abi-layout-$font_name.$font_shim_object_extension"
        repack_cross_archive \
            "$font_shim_archive" \
            "$font_destination/${font_prefix}SilexFont.$font_extension" \
            "$font_shim_object_extension" \
            "$font_name-SilexFont"

        font_upstream_suffix=o
        if [ "$font_system" = Windows ]; then
            font_upstream_suffix=obj
        fi
        repack_cross_archive \
            "$font_install_freetype/lib/libfreetype.a" \
            "$font_destination/${font_prefix}freetype.$font_extension" \
            "$font_upstream_suffix" \
            "$font_name-freetype"
        repack_cross_archive \
            "$font_install_harfbuzz/lib/libharfbuzz.a" \
            "$font_destination/${font_prefix}harfbuzz.$font_extension" \
            "$font_upstream_suffix" \
            "$font_name-harfbuzz"
    fi
}

build_target macos-arm64 Darwin arm64 cc c++ a lib
build_target linux-x64 Linux x86_64 "$font_native_dir/Toolchains/zig-cc-linux-x64" "$font_native_dir/Toolchains/zig-cxx-linux-x64" a lib
build_target windows-x64 Windows x86_64 "$font_native_dir/Toolchains/zig-cc-windows-x64" "$font_native_dir/Toolchains/zig-cxx-windows-x64" lib ""
build_target windows-arm64 Windows aarch64 "$font_native_dir/Toolchains/zig-cc-windows-arm64" "$font_native_dir/Toolchains/zig-cxx-windows-arm64" lib ""

echo "GFX.Font boundary archives rebuilt in $font_package_dir/Boundary"
