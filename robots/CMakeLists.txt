set(ROBOT_PLATFORM "" CACHE STRING "Robot Platform to use (check the files in the src/ directory for options)")

if(NOT ROBOT_PLATFORM)
    message(FATAL_ERROR "You need to select a robot platform")
else()
    message("Selected Robot Platform: ${ROBOT_PLATFORM}")
endif ()

string(TOLOWER "${ROBOT_PLATFORM}" ROBOT_PLATFORM_LOWER)

# Common Robot sources
file(GLOB ROBOT_COMMON_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/robot.cpp"
)

# Robot specific sources
file(GLOB ROBOT_PLATFORM_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/${ROBOT_PLATFORM_LOWER}_*.cpp"
)

# Zusammenführen und nach oben geben
set(PLATFORM_SOURCES
    ${ROBOT_COMMON_SOURCES}
    ${ROBOT_PLATFORM_SOURCES}
)
