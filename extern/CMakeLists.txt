include(CMakeProject-lua.cmake)
include(CMakeProject-glew.cmake)
include(CMakeProject-json.cmake)
include(CMakeProject-tomcrypt.cmake)
include(CMakeProject-tommath.cmake)
include(CMakeProject-miniz.cmake)

# When using vcpkg, skip libusb custom build (vcpkg provides it)
# When NOT using vcpkg, build libusb from source
if(NOT (DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake"))
  include(CMakeProject-libusb.cmake)
endif()
