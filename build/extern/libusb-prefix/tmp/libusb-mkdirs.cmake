# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/jas/source/repos/itgmania-testing/extern/libusb")
  file(MAKE_DIRECTORY "/home/jas/source/repos/itgmania-testing/extern/libusb")
endif()
file(MAKE_DIRECTORY
  "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/src/libusb-build"
  "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix"
  "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/tmp"
  "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/src/libusb-stamp"
  "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/src"
  "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/src/libusb-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/src/libusb-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jas/source/repos/itgmania-testing/build/extern/libusb-prefix/src/libusb-stamp${cfgdir}") # cfgdir has leading slash
endif()
