cmake_minimum_required(VERSION 3.20)

if(APPLE)
  if(NOT CMAKE_OSX_ARCHITECTURES)
    execute_process(
      COMMAND uname -m
      OUTPUT_VARIABLE MACHINE_ARCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(MACHINE_ARCH MATCHES "arm64")
      set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architecture" FORCE)
    elseif(MACHINE_ARCH MATCHES "x86_64")
      set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "macOS architecture" FORCE)
    else()
      message(FATAL_ERROR "Unsupported macOS architecture: ${MACHINE_ARCH}")
    endif()
  endif()
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
  else()
    message(WARNING "VCPKG_ROOT environment variable not set. Set it or pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake")
  endif()
endif()

project(StepMania)

include(StepmaniaCore.cmake)

if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
  message(STATUS "Using vcpkg for package management (FFmpeg uses custom build)")
  
  find_package(GLEW REQUIRED)
  find_package(hidapi CONFIG REQUIRED)
  find_package(ixwebsocket REQUIRED)
  find_package(JPEG REQUIRED)
  find_package(PNG REQUIRED)
  find_package(mad CONFIG REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PCRE REQUIRED libpcre)
  
  # Libraries that may not have CMake config files, try pkg-config first
  pkg_check_modules(TOMCRYPT libtomcrypt)
  pkg_check_modules(TOMMATH libtommath)
  pkg_check_modules(LIBUSB libusb-1.0)
  
  find_package(MbedTLS REQUIRED)
  find_package(Ogg REQUIRED)
  find_package(Vorbis REQUIRED)
  find_package(ZLIB REQUIRED)
  find_package(jsoncpp CONFIG REQUIRED)
endif()

# The external libraries need to be included.
add_subdirectory(extern)

# The internal libraries and eventual executable to be used.
add_subdirectory(src)
