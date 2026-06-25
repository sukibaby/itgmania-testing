cmake_minimum_required(VERSION 3.20)

project(StepMania)

include(StepmaniaCore.cmake)

# The external libraries need to be included.
add_subdirectory(extern)

if(WITH_SDL_WINDOW)
  if(TARGET SDL3::SDL3)
    set(HAS_SDL TRUE)
    message(STATUS "Using bundled SDL3 submodule for window support")
  else()
    find_package(SDL2)
    if(SDL2_FOUND)
      set(HAS_SDL TRUE)
      message(STATUS "Using system SDL2 for window support")
    else()
      set(HAS_SDL FALSE)
      message(STATUS "SDL2 was not found on your system. SDL windowing support will not be available.")
    endif()
  endif()
else()
  set(HAS_SDL FALSE)
endif()

# The internal libraries and eventual executable to be used.
add_subdirectory(src)
