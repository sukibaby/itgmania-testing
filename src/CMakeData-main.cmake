list(APPEND SMDATA_MAIN_PROJECT_SRC
            "main/StepManiaMain.cpp")

if(NOT APPLE)
  list(APPEND SMDATA_MAIN_PROJECT_SRC "Main.cpp")
endif()

list(APPEND SMDATA_MAIN_PROJECT_HPP)

source_group("Main Project"
             FILES
             ${SMDATA_MAIN_PROJECT_SRC}
             ${SMDATA_MAIN_PROJECT_HPP})