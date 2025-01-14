add_subdirectory(ChibiOS_21.11.3)

set(XBOT_BUILD_LIB_SERVICE ON)
set(XBOT_BUILD_LIB_SERVICE_INTERFACE OFF)
add_subdirectory(xbot_framework)

target_link_libraries(xbot-service PUBLIC ChibiOS)
target_compile_definitions(xbot-service PUBLIC XBOT_ENABLE_STATIC_STACK)

add_subdirectory(LSM6DS3TR-C-PID)
add_subdirectory(etl)
