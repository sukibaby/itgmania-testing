# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/jas/source/repos/itgmania-testing/extern/libjpeg-turbo")
  file(MAKE_DIRECTORY "/home/jas/source/repos/itgmania-testing/extern/libjpeg-turbo")
endif()
file(MAKE_DIRECTORY
  "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/src/libjpeg_turbo_project-build"
  "/home/jas/source/repos/itgmania-testing/build/libjpeg-turbo-install"
  "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/tmp"
  "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/src/libjpeg_turbo_project-stamp"
  "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/src"
  "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/src/libjpeg_turbo_project-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/src/libjpeg_turbo_project-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jas/source/repos/itgmania-testing/build/extern/libjpeg_turbo_project-prefix/src/libjpeg_turbo_project-stamp${cfgdir}") # cfgdir has leading slash
endif()
