## Install CMake and Git

The common way of installing CMake and Git is to install it via your distribution's package manager.

For example, on Ubuntu the command would be

`sudo apt-get install -y cmake git`

## Install other prerequisites

Generally, a few other packages are required besides CMake and git to be able to start the build.

On Debian/Ubuntu, the needed packages are:

 - autoconf
 - autoconf-archive
 - automake
 - libtool
 - pkg-config
 - libasound2-dev
 - libglu1-mesa-dev
 - libgtk-3-dev
 - libjack-dev
 - libpulse-dev
 - libudev-dev
 - libxinerama-dev
 - libx11-dev
 - libxrandr-dev
 - libxtst-dev
 - nasm

`sudo apt-get install -y libasound2-dev libglu1-mesa-dev libgtk-3-dev libjack-dev libpulse-dev libudev-dev libxinerama-dev libx11-dev libxrandr-dev libxtst-dev nasm autoconf autoconf-archive automake libtool pkg-config`

Pre-made lists of install commands for non-Debian-based distributions are available here: https://github.com/itgmania/itgmania/discussions/403

## Building with vcpkg

### Prerequisite setup

1. **Install vcpkg** (if not already installed):
   ```bash
   git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
   cd ~/vcpkg
   ./bootstrap-vcpkg.sh
   ```

2. **Set environment variable**:
   ```bash
   export vcpkg_ROOT=$HOME/vcpkg
   ```

3. **Run the build commands**:
    ```bash
    cmake -B Build -DSM_USE_VCPKG=ON -DCMAKE_TOOLCHAIN_FILE=$vcpkg_ROOT/scripts/buildsystems/vcpkg.cmake -DWITH_FFMPEG_JOBS="$(nproc)"
    cmake --build Build --parallel "$(nproc)"
    ```

## Building without vcpkg

To build without vcpkg, you will need to ensure your git submodules are up to date before you can build. Enter the itgmania directory and run the following command:

```
git submodule update --init --recursive
```

You can then run `git submodule status` to be sure all your submodules are present. If they are, you can then run the following commands to build:

```bash
cmake -B Build -DSM_USE_VCPKG=OFF -DCMAKE_TOOLCHAIN_FILE= -DWITH_FFMPEG_JOBS="$(nproc)"
cmake --build Build --parallel "$(nproc)"
```

### Release vs Debug

If you are generating makefiles with cmake, you will also need to specify your build type.
Most users will want to use `RELEASE` while some developers may want to use `DEBUG`.

When generating your cmake files for the first time (or after any cache delete),
pass in `-DCMAKE_BUILD_TYPE=Debug` for a debug build. We have `RelWithDbgInfo` and `MinSizeRel` builds available as well.

It is advised to clean your cmake cache if you switch build types.

Note that if you use an IDE like Visual Studio, you do not need to worry about setting the build type.
You can edit the build type directly in the IDE.


## Installing ITGmania

Installing in this context refers to placing the folders and generated binary in a standard location based on your operating system.
This guide assumes default install locations.

If you want to change the initial location, pass in `-DCMAKE_INSTALL_PREFIX=/new/path/here` when configuring your local setup.

After installing, run `sudo make install`. The files will be placed in the location specified:
by default, that is now `/usr/local/itgmania`.

If you want to make a full build, you can run the `build-release-linux` script inside the `Utils` directory. You will need to freshly configure and generate build files after this.

### Last Words

With that, you should be good to go.
If there are still questions, view the resources on the parent directory's README.md file.
