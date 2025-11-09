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
  
  # On Windows, vcpkg triplet must be x64-windows-static
  if(WIN32 AND NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows-static")
    message(FATAL_ERROR "On Windows, you must use vcpkg with -DVCPKG_TARGET_TRIPLET=x64-windows-static. "
                        "Current triplet: ${VCPKG_TARGET_TRIPLET}")
  endif()
  
  find_package(GLEW CONFIG REQUIRED)
  find_package(hidapi CONFIG REQUIRED)
  find_package(ixwebsocket CONFIG REQUIRED)
  find_package(JPEG REQUIRED)
  find_package(PNG REQUIRED)
  find_package(mad CONFIG REQUIRED)
  find_package(miniz CONFIG REQUIRED)
  find_package(jsoncpp CONFIG REQUIRED)
  find_package(unofficial-pcre CONFIG REQUIRED)
  
  # Libraries that may not have CMake config files
  # Note: TOMCRYPT, TOMMATH, LIBUSB are optional and handled separately if needed
  
  find_package(MbedTLS REQUIRED)
  find_package(Ogg CONFIG REQUIRED)
  find_package(Vorbis CONFIG REQUIRED)
  find_package(ZLIB REQUIRED)
endif()

# The external libraries need to be included.
add_subdirectory(extern)

# The internal libraries and eventual executable to be used.
add_subdirectory(src)
