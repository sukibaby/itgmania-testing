# FFmpeg build for Windows

## Context

Historically, the Windows build of StepMania used precompiled FFmpeg binaries generated using mingw. This was necessary because FFmpeg can not be built using MSVC. However, the game must be built using MSVC for Windows, so preparing the FFmpeg binaries for Windows requires additional steps, which can be provided using any host OS which supports mingw and can build for the `mingw64` target.

To satisfy the dependencies for this build, mingw must compile bzip2 and zlib targeting x86_64-w64-mingw32 prior to compiling FFmpeg.

Currently, we use a GitHub Actions workflow to automate this process so that changes to the ffmpeg submodule are monitored, so that the Windows prebuilts can be kept up to date without manual intervention.


## Automated Build

The workflow `.github\workflows\ffmpeg-w32.yml` exists to automatically rebuild and publish the Windows `ffmpeg-w32` prebuilts automatically following any FFmpeg-related source changes. Because FFmpeg cannot be compiled with MSVC, the workflow uses MinGW-w64 on Ubuntu to cross-compile the libraries for Windows. It also builds the required dependencies (zlib and bzip2) so the resulting binaries are self-contained.

After building, the workflow packages the DLLs, import libraries, and headers into `extern/ffmpeg-w32`, uploads them as an artifact, and commits the updated prebuilts and generated headers back to `beta` if anything changed. Thus our Windows FFmpeg binaries for Windows stay in sync with the FFmpeg submodule without requiring manual rebuilds.

Please note that if the compiled headers change significantly, the CI/Release runs may fail until the ffmpeg-w32 rebuild has had a chance to finish and make its commit directly to the `beta` branch, so if possible, it's best to let any FFmpeg changes complete before running a CI or automated release. 

## Manual Build

Ubuntu/Debian users can generally reference the commands from ffmpeg-w32.yml with minimal changes, as the workflow is similarly structured to a bash script.

### Prerequisites

Before we can build the Windows FFmpeg binaries, we need to also build bzip2 and zlib for x86_64-w64-mingw32. This can be done on Debian/Linux like so:

From the zlib directory:
```bash
make -f win32/Makefile.gcc PREFIX=x86_64-w64-mingw32- SHARED_MODE=1
sudo make -f win32/Makefile.gcc install DESTDIR=/usr/x86_64-w64-mingw32/ INCLUDE_PATH=/include LIBRARY_PATH=/lib BINARY_PATH=/bin SHARED_MODE=1
```

From the bzip2 directory:
```bash
make CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib libbz2.a
sudo cp libbz2.a /usr/x86_64-w64-mingw32/lib/
sudo cp bzlib.h /usr/x86_64-w64-mingw32/include/
make clean
```

### Building FFmpeg

If you are on a Debian based distro, you can basically copy the commands directly from `ffmpeg-w32.yml`.

An example command to manually build these binaries on Arch Linux:
```bash
# pacman -S mingw-w64-{binutils,crt,gcc,headers,winpthreads}
# pamac install mingw-w64-{bzip2,zlib,pkg-config}

./configure \
	--arch=x86_64 \
	--target-os=mingw32 \
	--cross-prefix=x86_64-w64-mingw32- \
	--disable-runtime-cpudetect \
	--disable-autodetect \
	--disable-asm \
	--disable-avdevice \
	--disable-avfilter \
	--disable-debug \
	--disable-devices \
	--disable-doc \
	--disable-encoders \
	--disable-faan \
	--disable-filters \
	--disable-hwaccels \
	--disable-iamf \
	--disable-lsp \
	--disable-lzma \
	--disable-muxers \
	--disable-network \
	--disable-pixelutils \
	--disable-programs \
	--disable-static \
	--disable-swresample \
	--enable-bzlib \
	--enable-d3d11va \
	--enable-dxva2 \
	--enable-gpl \
	--enable-version3 \
	--enable-shared \
	--enable-w32threads \
	--enable-zlib \
	--extra-cflags=-w \
	--extra-ldflags=-static \
	--prefix=/
```

Note that we explicitly disable building with optimizations to ensure compatibility with all hosts for the official build, but you can enable optimizations for your platform if you're making a local build.	

### Copying the files

FFmpeg will generate an identical pair of DLL's where one has a version number, and the other does not. ITGmania only uses the DLL containing a version number. 

Lastly, the headers need to be copied into the `extern/ffmpeg-w32/include` directory. Be sure to also check for generated headers such as `avconfig.h`. The headers in the include directory must match the headers from the version of ffmpeg that was build.