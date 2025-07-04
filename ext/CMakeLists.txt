add_subdirectory(ChibiOS_21.11.3)

set(XBOT_BUILD_LIB_SERVICE ON)
set(XBOT_BUILD_LIB_SERVICE_INTERFACE OFF)
add_subdirectory(xbot_framework)

target_link_libraries(xbot-service PUBLIC ChibiOS etl::etl)
target_compile_definitions(xbot-service PUBLIC XBOT_ENABLE_STATIC_STACK)

add_subdirectory(LSM6DS3TR-C-PID)
add_subdirectory(etl)

# LVGL Configuration - Disable examples and demos to reduce compile time and binary size
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable LVGL examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable LVGL demos" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "Disable LVGL examples build" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "Disable LVGL demos build" FORCE)

# Add LVGL only for specific robots
if(ROBOT_PLATFORM STREQUAL "Sabo")
    add_subdirectory(lvgl)
endif()

# Add LittleFS
add_library(LittleFS littlefs/lfs.c littlefs/lfs_util.c)
target_include_directories(LittleFS PUBLIC littlefs ../cfg)
target_compile_definitions(LittleFS PUBLIC LFS_DEFINES=lfs_config.h)
target_link_libraries(LittleFS PUBLIC ulog ChibiOS etl::etl)