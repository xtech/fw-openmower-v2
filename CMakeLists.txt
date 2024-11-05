cmake_minimum_required(VERSION 3.22)

set(CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cfg)
set(BOARD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/boards/XCORE)
# Set custom port path
set(XBOT_CUSTOM_PORT_PATH ${CMAKE_SOURCE_DIR}/portable)


# Setup compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)



# Set the project name
set(CMAKE_PROJECT_NAME openmower)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})


# Add lib subdirectory
add_subdirectory(ext)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        main.cpp
        src/globals.c
        src/globals.h
        src/heartbeat.c
        src/heartbeat.h
        src/id_eeprom.c
        src/id_eeprom.h
        src/boot_service_discovery.c
        src/boot_service_discovery.h
        src/status_led.c
        src/status_led.h
        src/services/imu_service/imu_service.cpp
        src/services/imu_service/imu_service.hpp
        src/services/power_service/power_service.cpp
        src/services/power_service/power_service.hpp
        # BQ2567 driver
        src/drivers/bq_2576/bq_2576.cpp
        src/drivers/bq_2576/bq_2576.hpp
        # VESC driver
        src/drivers/vesc/buffer.cpp
        src/drivers/vesc/VescUart.cpp
        src/drivers/vesc/crc.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        src
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
        # Add user defined libraries
        ChibiOS
        LSM6DS3TR
)

target_add_service(${CMAKE_PROJECT_NAME} ImuService ${CMAKE_CURRENT_SOURCE_DIR}/services/imu_service.json)
target_add_service(${CMAKE_PROJECT_NAME} PowerService ${CMAKE_CURRENT_SOURCE_DIR}/services/power_service.json)

set_target_properties(${CMAKE_PROJECT_NAME}
        PROPERTIES SUFFIX ".elf")

add_custom_command(TARGET ${CMAKE_PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Obinary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -Oihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} --pad-to=0x8020000 -O binary ${CMAKE_CURRENT_SOURCE_DIR}/bootloader/xcore-boot-unsafe.elf ${CMAKE_PROJECT_NAME}-with-unsafe-bootloader.bin && cat ${CMAKE_PROJECT_NAME}.bin>>${CMAKE_PROJECT_NAME}-with-unsafe-bootloader.bin
)
