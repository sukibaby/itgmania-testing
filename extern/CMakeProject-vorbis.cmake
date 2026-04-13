set(VORBIS_ROOT "${SM_EXTERN_DIR}/vorbis")

sm_glob_files(VORBIS_SRC
              BASE_DIR "${VORBIS_ROOT}/lib"
              PATTERNS "*.c"
              EXCLUDE_PATTERNS
                "barkmel.c"
                "misc.c"
                "psytune.c"
                "tone.c"
                "vorbisfile.c")
sm_glob_files(VORBIS_HPP
              RECURSE
              BASE_DIR "${VORBIS_ROOT}"
              PATTERNS "lib/*.h" "include/vorbis/*.h")

source_group(TREE "${VORBIS_ROOT}" FILES ${VORBIS_SRC} ${VORBIS_HPP})

add_library("vorbis" STATIC ${VORBIS_SRC} ${VORBIS_HPP})

set_property(TARGET "vorbis" PROPERTY FOLDER "External Libraries")

disable_project_warnings("vorbis")

target_include_directories("vorbis" PUBLIC
  "${SM_EXTERN_DIR}/ogg"
  "${VORBIS_ROOT}/lib"
  "${VORBIS_ROOT}/include"
)

target_link_libraries("vorbis" "ogg")
