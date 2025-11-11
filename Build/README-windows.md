## Install CMake

The common way of installing CMake is to go to [CMake's download page](http://www.cmake.org/download/). Download and install the binary for Windows with GUI.

## Install Git

If you do not have Git, install the latest release from [git-scm.com](https://git-scm.com/install/windows).

## Building with vcpkg (easiest)

### Prerequisite setup

Open a command line, such as Windows Terminal or PowerShell.

1. **Install vcpkg** (if not already installed):
   ```bash
   git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
   cd ~/vcpkg
   ./bootstrap-vcpkg.bat
   ```

2. **Set environment variable**:
   ```bash
   export vcpkg_ROOT=$HOME/vcpkg
   ```

3. **Configure the build**:
    ```bash
   cmake -B build -DSM_USE_VCPKG=ON -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DWITH_FFMPEG_JOBS="%NUMBER_OF_PROCESSORS%"
    ```

4. **Build the game**:
    ```bash
   cmake --build build
   ```

## Building without vcpkg

This step also assumes you have [Git SCM](https://git-scm.com/install/windows) installed.

You will need to ensure your git submodules are up to date before you can build. Enter the itgmania directory and run the following command using Git Bash:

```
git submodule update --init --recursive
```

You can then run `git submodule status` to be sure all your submodules are present. If they are, you can then run the following commands to build:

```bash
cmake -B Build -DSM_USE_VCPKG=OFF -DCMAKE_TOOLCHAIN_FILE= -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build Build
```

## Building with Visual Studio

The official ITGmania builds are made using Visual Studio 2022, but also work with Visual Studio 2026.

### CMake GUI

--------------------

For those that use the GUI to work with cmake, you need to specify where the source code is and where the binaries will be built.

For the source code location (_"Browse Source..."_), choose the `itgmania` directory.

For the binary location (_"Browse Build..."_), choose the `Build` directory. This is inside the `itgmania` folder you selected in the previous step.

Upon setting the source and build directories, you should `Configure` the build.
If no errors show up, you can hit `Generate` until the "Open Project" button is available. Once it is done generating the project files, click "Open Project" to open Visual Studio.

If the CMake project file changes, you can just generate the build to get up to date.
If this by itself doesn't work, you may have to clean the cmake cache.
Go to File -> Delete Cache, and then run the `Configure` and `Generate` steps again.

### Release vs Debug

--------------------

If you are generating makefiles with cmake, you will also need to specify your build type.
Most users will want to use `RELEASE` while some developers may want to use `DEBUG`.

When generating your cmake files for the first time (or after any cache delete),
pass in `-DCMAKE_BUILD_TYPE=Debug` for a debug build. We have `RelWithDbgInfo` and `MinSizeRel` builds available as well.

It is advised to clean your cmake cache if you switch build types.

Note that if you use an IDE like Visual Studio, you do not need to worry about setting the build type.
You can edit the build type directly in the IDE.

## Compiling ITGmania

Everything needed for building on Windows is available via the Visual Studio Installer:
- The "Desktop development with C++" workload (which includes MSVC, C++ ATL, C++ MFC, C++ modules, C++/CLI support),
- The Windows SDK.

Upon launching the Visual Studio Installer, you will be in the _Workloads_ section. Here, you should select "Desktop development with C++".

![image](https://github.com/user-attachments/assets/7ed2370d-c891-4d7b-9ca5-c9ad39ad969c)

The **Windows SDK** is also required, so click the _Individual components_ section. Either use the search, or scroll thru the list until you find the section titled _SDK's, libraries, and frameworks_ to find the Windows SDK.

![image](https://github.com/user-attachments/assets/8cf1b443-bfbe-4c2f-af39-a904324956f5)

Confirm the installation. Once everything is installed, you can open **StepMania.sln** with Visual Studio. Set the build type to **Release** and press the hollow green button (or Ctrl+F5) to begin building.

![image](https://github.com/user-attachments/assets/f9235e14-bfc8-4f8f-8b30-9706dfb3bcc6)

## Installing ITGmania

Installing in this context refers to placing the folders and generated binary in a standard location based on your operating system.
This guide assumes default install locations.

If you want to change the initial location, pass in `-DCMAKE_INSTALL_PREFIX=/new/path/here` when configuring your local setup.

#### Windows

If you want to make a full build, there are two options. One is to run the `build-release-windows` script inside the `Utils` directory. The other is to select `WITH_FULL_RELEASE` in the CMake GUI. You will need to freshly configure and generate build files after this.

You will need to have [NSIS](https://nsis.sourceforge.io/Download) installed in order to generate a full build on Windows.

The default installation directory is `C:\Games\ITGmania`.

### Last Words

With that, you should be good to go.
If there are still questions, view the resources on the parent directory's README.md file.
