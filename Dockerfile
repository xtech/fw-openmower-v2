FROM ubuntu:24.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi git \
    libasio-dev iproute2 \
    python3 python3-venv python3-pip \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

# venv because 24.04 marks the system python as externally managed (PEP 668)
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir elf-size-analyze
ENV PATH="/opt/venv/bin:${PATH}"

# Build preset to use for all platforms. CI passes "Debug" for pull requests and
# "Release" for main / tagged releases.
ARG BUILD_PRESET=Release

####################################################################################################
# Fetch the third-party sources in their own layer. Its only input is
# cmake/deps.cmake, so editing firmware code reuses the layer; only a dependency
# change invalidates it. Before, the build below cloned LVGL (689 MB checkout) and
# ETL again on every commit.
FROM builder AS deps

COPY cmake/deps.cmake cmake/gcc-arm-none-eabi.cmake /project/cmake/
COPY docker/deps/CMakeLists.txt /project/CMakeLists.txt
# LVGL's CMake insists that a lv_conf.h exists next to the top-level project and
# only checks for its presence at configure time. A placeholder keeps lv_conf.h
# out of this layer's cache key: tweaking the UI config must not re-download the
# dependencies (the real build brings its own lv_conf.h).
RUN touch /project/lv_conf.h
RUN cmake -S /project -B /project/build \
        -DCMAKE_TOOLCHAIN_FILE=/project/cmake/gcc-arm-none-eabi.cmake \
        -DFETCHCONTENT_BASE_DIR=/deps

####################################################################################################
FROM builder AS build

# Firmware version string (`git describe --tags --dirty --always`). The build
# context excludes .git, so CI computes it on the runner and passes it in; a local
# `docker build` without it gets "unknown" (see cmake/GetGitVersion.cmake).
# Declared here, in the volatile stage, and not in "builder": an ARG that changes
# on every commit must not be able to invalidate the cached deps stage above.
ARG BUILD_PRESET=Release
ARG FW_VERSION=""

# The sources come from the layer above. FETCHCONTENT_UPDATES_DISCONNECTED stops
# ETL's git from running fetch/checkout again on the already pinned clone.
COPY --from=deps /deps /deps
ENV FETCHCONTENT_BASE_DIR=/deps

COPY . /project

WORKDIR /project

RUN mkdir -p build

RUN cd build && cmake .. --preset=${BUILD_PRESET} -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -DFW_VERSION="${FW_VERSION}" -B${BUILD_PRESET} && cd ${BUILD_PRESET} && make -j$(nproc)
RUN elf-size-analyze -H -R -t arm-none-eabi- ./build/${BUILD_PRESET}/openmower-firmware.elf -W > build/ram-info.html
RUN elf-size-analyze -H -F -t arm-none-eabi- ./build/${BUILD_PRESET}/openmower-firmware.elf -W > build/flash-info.html

FROM scratch
ARG BUILD_PRESET=Release
COPY --from=build /project/build/ram-info.html /ram-info.html
COPY --from=build /project/build/flash-info.html /flash-info.html
COPY --from=build /project/build/${BUILD_PRESET}/openmower-firmware.bin /openmower-firmware.bin
COPY --from=build /project/build/${BUILD_PRESET}/openmower-firmware.elf /openmower-firmware.elf
