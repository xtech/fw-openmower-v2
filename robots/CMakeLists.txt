set(ROBOT_PLATFORM "" CACHE STRING "Robot Platform to use (check the files in the src/ directory for options)")

if(NOT ROBOT_PLATFORM)
    message(FATAL_ERROR "You need to select a robot platform")
else()
    message("Selected Robot Platform: ${ROBOT_PLATFORM}")
endif ()

# find the platform sources and add them
file(GLOB_RECURSE PLATFORM_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/*")
