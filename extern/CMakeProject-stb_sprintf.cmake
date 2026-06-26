set(STB_SPRINTF_SRC "${SM_EXTERN_DIR}/stb_sprintf.c")
set(STB_SPRINTF_HPP "${SM_EXTERN_DIR}/stb/stb_sprintf.h")

source_group(TREE "${SM_EXTERN_DIR}" FILES ${STB_SPRINTF_SRC} ${STB_SPRINTF_HPP})

add_library("STB_SPRINTF" STATIC ${STB_SPRINTF_SRC} ${STB_SPRINTF_HPP})

set_property(TARGET "STB_SPRINTF" PROPERTY FOLDER "External Libraries")

target_include_directories("STB_SPRINTF" PUBLIC "${SM_EXTERN_DIR}/stb")