set(OGG_ROOT "${SM_EXTERN_DIR}/ogg")
set(OGG_GENERATED_ROOT "${CMAKE_CURRENT_BINARY_DIR}/ogg")
set(OGG_GENERATED_HPP "${OGG_GENERATED_ROOT}/ogg/config_types.h")

sm_glob_files(OGG_SRC BASE_DIR "${OGG_ROOT}" PATTERNS "src/*.c")
sm_glob_files(OGG_SOURCE_HPP BASE_DIR "${OGG_ROOT}" PATTERNS "src/*.h" "include/ogg/*.h")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.ogg.types.in.h" "${OGG_GENERATED_HPP}")

set(OGG_HPP ${OGG_SOURCE_HPP} "${OGG_GENERATED_HPP}")

source_group(TREE "${OGG_ROOT}" FILES ${OGG_SRC} ${OGG_SOURCE_HPP})
source_group("Generated Files" FILES "${OGG_GENERATED_HPP}")

add_library("ogg" STATIC ${OGG_SRC} ${OGG_HPP})

set_property(TARGET "ogg" PROPERTY FOLDER "External Libraries")

disable_project_warnings("ogg")

target_include_directories("ogg" PUBLIC
  "${OGG_ROOT}/include"
  "${OGG_GENERATED_ROOT}"
)
