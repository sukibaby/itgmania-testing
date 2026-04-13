set(LUA_ROOT "${SM_EXTERN_DIR}/lua-5.1")
set(LUA_SRC_DIR "${LUA_ROOT}/src")

sm_glob_files(LUA_SRC
        BASE_DIR "${LUA_SRC_DIR}"
        PATTERNS "*.c"
        EXCLUDE_PATTERNS "lua.c" "luac.c" "print.c")
sm_glob_files(LUA_HPP BASE_DIR "${LUA_SRC_DIR}" PATTERNS "*.h")

source_group(TREE "${LUA_ROOT}" FILES ${LUA_SRC} ${LUA_HPP})

add_library("lua-5.1" STATIC ${LUA_SRC} ${LUA_HPP})

set_property(TARGET "lua-5.1" PROPERTY FOLDER "External Libraries")

# include_directories(src)

if(MSVC)
  target_compile_definitions("lua-5.1" PRIVATE _CRT_SECURE_NO_WARNINGS)
  set_source_files_properties(${LUA_SRC} PROPERTIES LANGUAGE CXX)
endif(MSVC)

disable_project_warnings("lua-5.1")
