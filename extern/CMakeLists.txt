include(CMakeProject-lua.cmake)
include(CMakeProject-tomcrypt.cmake)
include(CMakeProject-tommath.cmake)

# When using vcpkg, skip libraries provided by vcpkg
if(NOT (DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake"))
  include(CMakeProject-glew.cmake)
  include(CMakeProject-json.cmake)
  include(CMakeProject-miniz.cmake)
  include(CMakeProject-libusb.cmake)
  message(STATUS "Building custom libraries: glew, jsoncpp, miniz, libusb")
else()
  # vcpkg provides: glew, jsoncpp, miniz, libusb
  # Custom builds: lua-5.1, tomcrypt, tommath (tommath has CMake issues in vcpkg)
  message(STATUS "Using vcpkg packages for: glew, jsoncpp, miniz, libusb")
endif()
