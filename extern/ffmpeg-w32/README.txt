NOTE(sukibaby):

Certain Windows FFmpeg build flags differ from the Linux/macOS builds (see CMake/SetupFfmpeg.cmake)

Of course, for some of these, it can't be helped:
- Windows uses --enable-shared,     Linux/macOS uses --enable-static
- Windows uses --enable-w32threads, Linux/macOS uses --enable-pthreads
- Windows enables --enable-bzlib,   Linux/macOS disables it
- Windows enables D3D11VA/DXVA2,    macOS uses videotoolbox

To improve consistency between the Windows and macOS/Linux ports, consider adding --disable-debug
and --extra-cflags=-w to the Windows build. If updating these build flags, it will be important to
ensure that the resulting DLLs remain compatible with the Windows linker flags in src/CMakeLists.txt.

===================

The binaries in this folder where built on arch linux with mingw.

# pacman -S mingw-w64-{binutils,crt,gcc,headers,winpthreads}
# pamac install mingw-w64-{bzip2,zlib,pkg-config}

./configure \
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

./configure \
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
