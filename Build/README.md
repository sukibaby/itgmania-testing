# Instructions

Check the README file which pertains to the OS you are using for build instructions.

Most users will find everything they need in these README files. There are significant differences in the build process for ITGmania as compared to Stepmania, so it is recommended to stick with these ITGmania instructions.

For anything not covered in this guide, you can often refer to the original Stepmania documents [here](https://github.com/stepmania/stepmania/wiki/Compiling-StepMania), as long as you replace references to `Stepmania` with `ITGmania`. However, much of it is aged enough to be irrelevant at this point.

## Continuous Integration
Pushes to the `beta` branch of the repository generate a new build from the ITGmania [GitHub Actions workflow](https://github.com/itgmania/itgmania/actions/workflows/release.yml). This generates full installers for for Linux x86_64, Linux arm64, macOS x86_64, macOS arm64, and Windows x86_64.

By default, GitHub stores [auto-generated builds](https://github.com/itgmania/itgmania/actions/workflows/nightly.yml) for 90 days. People who are signed into GitHub can download these builds.

## Prerequisites

The following tools are needed:

 - CMake
    - Available from [cmake.org](https://cmake.org/download/), or via most package managers
 - vcpkg
    - Typically installed via `git clone`, but also available via package managers
 - A compiler
    - For Windows, use Visual Studio
    - For Mac, the Xcode Command Line Tools are all you need, but you can get also install Xcode via the Mac App Store to automatically install everything needed
    - For Linux, gcc is recommended. The officially supported version of gcc aligns with the default gcc version Ubuntu release targeted at the current ITGmania version. However, any recent version of gcc should work.