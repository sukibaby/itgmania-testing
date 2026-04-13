set(TOMMATH_ROOT "${SM_EXTERN_DIR}/libtommath")

sm_glob_files(TOMMATH_SRC BASE_DIR "${TOMMATH_ROOT}" PATTERNS "*.c")
sm_glob_files(TOMMATH_HPP BASE_DIR "${TOMMATH_ROOT}" PATTERNS "*.h")

source_group(TREE "${TOMMATH_ROOT}" FILES ${TOMMATH_SRC} ${TOMMATH_HPP})

add_library("tommath" STATIC ${TOMMATH_SRC} ${TOMMATH_HPP})

set_property(TARGET "tommath" PROPERTY FOLDER "External Libraries")

# tommath 1.2.0 depends on dead code elimination
if(MSVC)
  # with MSVC the only way I could find to enable dead code elimination is to
  # enable optimization altogether. This requires to disable RTC (run-time
  # error checks) though, because those two options are not compatible.
  string(REGEX REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
  target_compile_options("tommath" PRIVATE "/Ox")
else()
  target_compile_options("tommath" PRIVATE "-Og")
endif()

disable_project_warnings("tommath")

target_include_directories("tommath" PUBLIC "${TOMMATH_ROOT}")
