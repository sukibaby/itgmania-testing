set(EXAMPLE OFF CACHE BOOL "Build libmad's example executable (libmad internal option)" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/libmad"
                 "${CMAKE_CURRENT_BINARY_DIR}/libmad"
                 EXCLUDE_FROM_ALL)

set(MAD_TARGETS "mad" "mad-static")

foreach(MAD_TARGET IN LISTS MAD_TARGETS)
  set_property(TARGET "${MAD_TARGET}" PROPERTY FOLDER "External Libraries")
  disable_project_warnings("${MAD_TARGET}")
endforeach()

if(MSVC)
  foreach(MAD_TARGET IN LISTS MAD_TARGETS)
    target_compile_definitions("${MAD_TARGET}" PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions("${MAD_TARGET}" PRIVATE inline=__inline)
  endforeach()
endif(MSVC)