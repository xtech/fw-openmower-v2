add_subdirectory(ChibiOS_21.11.3)

set(XBOT_BUILD_LIB_SERVICE ON)
set(XBOT_BUILD_LIB_SERVICE_INTERFACE OFF)
add_subdirectory(xbot_framework)

target_link_libraries(xbot-service PUBLIC ChibiOS etl::etl)
target_compile_definitions(xbot-service PUBLIC XBOT_ENABLE_STATIC_STACK)

add_subdirectory(LSM6DS3TR-C-PID)
add_subdirectory(etl)


# Add LittleFS
add_library(LittleFS littlefs/lfs.c littlefs/lfs_util.c)
target_include_directories(LittleFS PUBLIC littlefs ../cfg)
target_compile_definitions(LittleFS PUBLIC LFS_DEFINES=lfs_config.h)
target_link_libraries(LittleFS PUBLIC ulog ChibiOS etl::etl)

add_subdirectory(u8g2)
target_compile_definitions(u8g2 PUBLIC U8X8_WITH_USER_PTR)