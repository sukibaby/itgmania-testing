set(SM_USING_VCPKG FALSE)
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
  set(SM_USING_VCPKG TRUE)
endif()

# Always build these because we rely on local forks or custom options.
set(SM_EXTERN_PROJECTS_ALWAYS
  CMakeProject-lua.cmake
  CMakeProject-libusb.cmake
)

# Build tomcrypt and tommath only if not using vcpkg
if(NOT SM_USING_VCPKG)
  list(APPEND SM_EXTERN_PROJECTS_ALWAYS
    CMakeProject-tomcrypt.cmake
    CMakeProject-tommath.cmake
  )
endif()

foreach(project ${SM_EXTERN_PROJECTS_ALWAYS})
  include(${project})
endforeach()

if(NOT SM_USING_VCPKG)
  set(SM_EXTERN_PROJECTS_CUSTOM
    CMakeProject-hidapi.cmake
    CMakeProject-glew.cmake
    CMakeProject-json.cmake
    CMakeProject-mad.cmake
    CMakeProject-zlib.cmake
    CMakeProject-libjpeg-turbo.cmake
    CMakeProject-ogg.cmake
    CMakeProject-vorbis.cmake
    CMakeProject-vorbisfile.cmake
    CMakeProject-pcre.cmake
    CMakeProject-png.cmake
    CMakeProject-ixwebsocket.cmake
    CMakeProject-miniz.cmake
  )

  foreach(project ${SM_EXTERN_PROJECTS_CUSTOM})
    include(${project})
  endforeach()
endif()
