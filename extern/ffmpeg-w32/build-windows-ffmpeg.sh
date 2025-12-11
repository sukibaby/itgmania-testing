#!/bin/sh
# Build FFmpeg libraries for Windows.
# intended to be run from an Arch Linux system.

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FFMPEG_SRC="${SCRIPT_DIR}/ffmpeg"
BUILD_DIR="${SCRIPT_DIR}/build"

# validate FFmpeg source directory
if [ ! -d "$FFMPEG_SRC" ]; then
    echo "Error: FFmpeg source not found at $FFMPEG_SRC !!"
    exit 1
fi

echo " - Checking for missing dependencies..."
sudo pacman -S --needed --noconfirm mingw-w64-binutils mingw-w64-crt mingw-w64-gcc mingw-w64-headers mingw-w64-winpthreads nasm
yay -S --needed --noconfirm mingw-w64-zlib mingw-w64-bzip2 mingw-w64-pkg-config

# X86_64 Build
echo " - Building x86_64 (64-bit) Windows FFmpeg..."
mkdir -p "${BUILD_DIR}/x64"
cd "${BUILD_DIR}/x64"

"${FFMPEG_SRC}/configure" \
    --arch=x86_64 \
    --target-os=mingw32 \
    --cross-prefix=x86_64-w64-mingw32- \
    --disable-autodetect \
    --disable-avdevice \
    --disable-avfilter \
    --disable-debug \
    --disable-devices \
    --disable-doc \
    --disable-filters \
    --disable-lzma \
    --disable-network \
    --disable-postproc \
    --disable-programs \
    --disable-static \
    --disable-swresample \
    --enable-bzlib \
    --enable-d3d11va \
    --enable-dxva2 \
    --enable-gpl \
    --enable-shared \
    --enable-w32threads \
    --enable-zlib \
    --extra-cflags=-w \
    --extra-ldflags=-static \
    --prefix=/

make -j$(nproc)

echo "X64 Build completed."

echo " - Removing duplicate x64 DLLs..."
# FFmpeg builds two sets of identical DLL's - we only use the version tagged DLL's
rm -f libavcodec/avcodec.dll libavformat/avformat.dll libavutil/avutil.dll libswscale/swscale.dll

echo " - Copying x86_64 binaries..."
mkdir -p "${SCRIPT_DIR}/x64"

cp -v libavcodec/*.dll libavformat/*.dll libavutil/*.dll libswscale/*.dll "${SCRIPT_DIR}/x64/" || true
cp -v libavcodec/*.def libavformat/*.def libavutil/*.def libswscale/*.def "${SCRIPT_DIR}/x64/" || true
cp -v libavcodec/*.lib libavformat/*.lib libavutil/*.lib libswscale/*.lib "${SCRIPT_DIR}/x64/" || true

echo "Done with x86_64."

# X86 Build (... do we use these?)
echo " - Building x86 (32-bit) Windows FFmpeg..."
mkdir -p "${BUILD_DIR}/x86"
cd "${BUILD_DIR}/x86"

"${FFMPEG_SRC}/configure" \
    --arch=x86 \
    --target-os=mingw32 \
    --cross-prefix=i686-w64-mingw32- \
    --disable-autodetect \
    --disable-avdevice \
    --disable-avfilter \
    --disable-debug \
    --disable-devices \
    --disable-doc \
    --disable-filters \
    --disable-lzma \
    --disable-network \
    --disable-postproc \
    --disable-programs \
    --disable-static \
    --disable-swresample \
    --enable-bzlib \
    --enable-d3d11va \
    --enable-dxva2 \
    --enable-gpl \
    --enable-shared \
    --enable-w32threads \
    --enable-zlib \
    --extra-cflags=-w \
    --extra-ldflags=-static \
    --prefix=/

make -j$(nproc)

echo " - Removing duplicate x86 DLLs..."
rm -f libavcodec/avcodec.dll libavformat/avformat.dll libavutil/avutil.dll libswscale/swscale.dll

echo " - Copying x86 binaries..."
mkdir -p "${SCRIPT_DIR}/x86"

cp -v libavcodec/*.dll libavformat/*.dll libavutil/*.dll libswscale/*.dll "${SCRIPT_DIR}/x86/" || true
cp -v libavcodec/*.def libavformat/*.def libavutil/*.def libswscale/*.def "${SCRIPT_DIR}/x86/" || true
cp -v libavcodec/*.lib libavformat/*.lib libavutil/*.lib libswscale/*.lib "${SCRIPT_DIR}/x86/" || true

echo "Done with x86."

# build success!
echo "FFmpeg-w32 build completed successfully."
echo "x86_64 DLLs: ${SCRIPT_DIR}/x64/"
echo "x86 DLLs:    ${SCRIPT_DIR}/x86/"
echo ""
echo " - Build artifacts are in: ${BUILD_DIR}"
