FROM ubuntu:24.04 AS builder
LABEL authors="Clemens Elflein"

# --no-install-recommends keeps the host gcc/g++, boost, X11 and friends out (137
# of 205 packages were only pulled in as recommendations). The target C/C++
# runtime is one of those recommendations of gcc-arm-none-eabi, so newlib and
# libstdc++ are listed explicitly; ca-certificates is needed for the https
# downloads of the deps stage.
#
# newlib, libstdc++ and libgcc ship prebuilt for 39 multilib variants (2.4 GB);
# the firmware links exactly one. Everything else is deleted in the same RUN, so
# it never ends up in a layer. The variant is asked from the compiler with the
# CPU flags of TARGET_FLAGS in cmake/gcc-arm-none-eabi.cmake - change them
# together: with other flags the link fails with "cannot find -lc" and friends.
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
    ca-certificates git \
    python3 python3-venv \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/* \
    && keep=$(arm-none-eabi-gcc -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -print-multi-directory) \
    && tops=$(arm-none-eabi-gcc -print-multi-lib | cut -d';' -f1 | cut -d/ -f1 | grep -vx '\.' | sort -u) \
    && for root in /usr/lib/arm-none-eabi/newlib "$(dirname "$(arm-none-eabi-gcc -print-libgcc-file-name)")"; do \
         test -d "$root/$keep" || { echo "multilib $keep missing in $root" >&2; exit 1; }; \
         for top in $tops; do \
           find "$root/$top" -path "$root/$keep" -prune -o ! -type d -exec rm -f {} +; \
           find "$root/$top" -type d -empty -delete; \
         done; \
       done

# venv because 24.04 marks the system python as externally managed (PEP 668)
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir elf-size-analyze
ENV PATH="/opt/venv/bin:${PATH}"

# No ARG in this stage: a stage built FROM another one inherits its build args,
# so an ARG here would become part of the cache key of every RUN in "deps" (a
# Debug build would then never reuse deps fetched by a Release build).

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

# Both build args are declared here, in the volatile stage, and not in "builder":
# they must not be able to invalidate the cached deps stage above.
# Build preset to use for all platforms. CI passes "Debug" for pull requests and
# "Release" for main / tagged releases.
ARG BUILD_PRESET=Release
# Firmware version string (`git describe --tags --dirty --always`). The build
# context excludes .git, so CI computes it on the runner and passes it in; a local
# `docker build` without it gets "unknown" (see cmake/GetGitVersion.cmake).
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
