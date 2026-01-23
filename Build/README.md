## Prerequisites

### macOS 
- **libtool** (required for libusb build): `brew install libtool`

---

## Setup Instructions

### 1. Clone and Bootstrap vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh (this is .bat on windows)
```

### 2. Set Environment Variable

Add vcpkg to your shell configuration:

**macOS/Linux (add to `~/.zshrc` or `~/.bashrc`):**
```bash
export vcpkg_ROOT=$HOME/vcpkg
```

**Windows (PowerShell):**
```powershell
$env:vcpkg_ROOT = "C:\path\to\vcpkg"
```

### 3. Install Dependencies

Navigate to the ITGmania source directory and install vcpkg packages:

**macOS (arm64):**
```bash
cd /path/to/itgmania-testing
$vcpkg_ROOT/vcpkg install --triplet arm64-osx
```

**macOS (x86_64):**
```bash
$vcpkg_ROOT/vcpkg install --triplet x64-osx
```

**Windows:**
```bash
cd \path\to\itgmania-testing
$env:vcpkg_ROOT\vcpkg install --triplet x64-windows-static
```

**Linux:**
```bash
cd /path/to/itgmania-testing
$vcpkg_ROOT/vcpkg install --triplet x64-linux
```

### 4. Configure with CMake

```bash
cmake -B Build -DSM_USE_VCPKG=ON -DCMAKE_TOOLCHAIN_FILE=$vcpkg_ROOT/scripts/buildsystems/vcpkg.cmake
```

### 5. Build

**macOS/Linux:**
```bash
cmake --build Build -j $(nproc)
```

**Windows:**
```bash
cmake --build Build -j %NUMBER_OF_PROCESSORS%
```

---

## Troubleshooting

### vcpkg_ROOT Not Set
Make sure the environment variable is set:
```bash
echo $vcpkg_ROOT  # macOS/Linux
echo $env:vcpkg_ROOT  # Windows PowerShell
```

### Clean Rebuild
If you encounter issues, try a clean rebuild:
```bash
rm -rf Build vcpkg_installed
```

---

## Building WITHOUT vcpkg

If you want to use the traditional custom build instead of vcpkg, you can build without vcpkg:

```bash
cmake -B Build -DSM_USE_VCPKG=OFF
cmake --build Build
```

---


## Continuous Integration

Pushes to the `beta` branch of the repository are built with a [GitHub Actions workflow](https://github.com/itgmania/itgmania/actions/workflows/release.yml), in which "nightly releases" are compiled for a matrix of operating systems and architectures. Full releases are built on pushses to the `release` branch.

By default, GitHub stores build artifacts for 90 days. You will need to be signed into GitHub to download one of these automated builds. They can be downloaded from the Artifacts section of the Summary page on an execution of the workflow.

If you click on _Actions_, you will see a list of workflows categories. One of the categories will be titled _Release_. Click on that, and then click on the most recent workflow (item with a green checkmark next to it). Scroll to the bottom of the page, where you will see a section called _Artifacts_ which contains the links to the downloads.
