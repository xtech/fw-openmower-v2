set(ROBOT_PLATFORM "" CACHE STRING "Robot Platform to use (check the subfolders in the src/ directory for options)")


if(NOT ROBOT_PLATFORM)
    message(FATAL_ERROR "You need to select a robot platform")
else()
    message("Selected Robot Platform: ${ROBOT_PLATFORM}")
endif ()

# find the platform sources and add them
file(GLOB_RECURSE PLATFORM_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/${ROBOT_PLATFORM}/*")

set(PLATFORM_HEADER_PATH "${CMAKE_CURRENT_LIST_DIR}/src/${ROBOT_PLATFORM}/robot_ex.hpp")
if(EXISTS ${PLATFORM_HEADER_PATH})
    message("Additional Robot Platform header found")
add_compile_definitions(-DROBOT_PLATFORM_HEADER="${PLATFORM_HEADER_PATH}")
else ()
    message("No Robot Platform header found")
endif ()

if(NOT PLATFORM_SOURCES)
message(FATAL_ERROR "No Platform Sources found, check your robot platform name!")
endif ()

message("Robot Platform Sources: ${PLATFORM_SOURCES}")
