## Installing XCode

You can either install the typical Xcode installation (~15GB) from the Mac App Store or the Apple Developer website, or the Xcode Command Line Tools (~3GB).

To install the full Xcode package, you can install it via the Mac App Store. It will require you to make an Apple Developer account if you do not have one already to complete the Xcode download.

Alternately, you can open a Terminal and type `xcode-select --install`. This will begin the installation for the Xcode Command Line Tools.

## Install CMake and git

The common way of installing CMake and Git is from [Homebrew](https://brew.sh). If you do not have Homebrew, install it and then install CMake and Git with:

`brew install cmake git`

## Building with vcpkg (easiest)

### Prerequisite setup

Open a command line, such as Windows Terminal or PowerShell.

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
    cmake -B Build -DCMAKE_TOOLCHAIN_FILE=$vcpkg_ROOT/scripts/buildsystems/vcpkg.cmake
    cmake --build Build
    ```

## Building with Xcode

If you wish to build with Xcode, you can use the following build command instead of step 3 above:

`cmake -G Xcode -B Build -DCMAKE_TOOLCHAIN_FILE=$vcpkg_ROOT/scripts/buildsystems/vcpkg.cmake`

You should then be able to find the Xcode project file inside the `build-xcode` directory.

### Release vs Debug

If you are generating makefiles with cmake, you will also need to specify your build type.
Most users will want to use `RELEASE` while some developers may want to use `DEBUG`.

When generating your cmake files for the first time (or after any cache delete),
pass in `-DCMAKE_BUILD_TYPE=Debug` for a debug build. We have `RelWithDbgInfo` and `MinSizeRel` builds available as well.

It is advised to clean your cmake cache if you switch build types.

Note that if you use an IDE like Xcode, you do not need to worry about setting the build type.
You can edit the build type directly in the IDE.

## Installing ITGmania

Installing in this context refers to placing the folders and generated binary in a standard location based on your operating system.
This guide assumes default install locations.

If you want to change the initial location, pass in `-DCMAKE_INSTALL_PREFIX=/new/path/here` when configuring your local setup.

The `ITGmania.app` package can be copied to `/Applications` and it will work as expected.

If you want to make a full build, there are two options. One is to run the `build-release-macos` script inside the `Utils` directory. The other is to pass the  `WITH_FULL_RELEASE` flag to CMake. You will need to freshly configure and generate build files after this.

The default installation directory is `C:\Games\ITGmania`.

### Last Words

With that, you should be good to go.
If there are still questions, view the resources on the parent directory's README.md file.

# Notes

As of ITGmania 1.2.0, ITGmania for macOS stores all user data in one directory:

 - `~/Library/Application Support/ITGmania`

In versions of ITGmania before 1.2.0, ITGmania kept user data in various directories:

 - `~/Pictures/ITGmania Screenshots`
 - `~/Library/Application Support/ITGmania`
 - `~/Library/Preferences/ITGmania`
 - `~/Library/Logs/ITGmania`
 - `~/Library/Caches/ITGmania`