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
