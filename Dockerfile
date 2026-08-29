FROM ubuntu:22.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi git \
    libasio-dev iproute2 \
    python3 python3-venv python3-pip \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

RUN pip install elf-size-analyze

# Build preset to use for all platforms. CI passes "Debug" for pull requests and
# "Release" for main / tagged releases.
ARG BUILD_PRESET=Release

COPY . /project

WORKDIR /project

# --- DEBUG (remove later): diagnose "-dirty" version string ---
RUN echo "=== DEBUG git rev-parse HEAD ==="; git rev-parse HEAD; echo "=== DEBUG git describe --tags --dirty --always ==="; git describe --tags --dirty --always; echo "=== DEBUG git status --porcelain ==="; git status --porcelain; echo "=== DEBUG git submodule status ==="; git submodule status; echo "=== DEBUG git log --oneline -3 ==="; git log --oneline -3; echo "=== DEBUG git tag --list ==="; git tag --list | tail -20

RUN mkdir build

# Share one FetchContent download/source dir so deps are fetched once.
ENV FETCHCONTENT_BASE_DIR=/project/build/_deps
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -B${BUILD_PRESET} && cd ${BUILD_PRESET} && make -j$(nproc)
RUN elf-size-analyze -H -R -t arm-none-eabi- ./build/${BUILD_PRESET}/openmower-firmware.elf -W > build/ram-info.html
RUN elf-size-analyze -H -F -t arm-none-eabi- ./build/${BUILD_PRESET}/openmower-firmware.elf -W > build/flash-info.html

FROM scratch
ARG BUILD_PRESET=Release
COPY --from=builder /project/build/ram-info.html /ram-info.html
COPY --from=builder /project/build/flash-info.html /flash-info.html
COPY --from=builder /project/build/${BUILD_PRESET}/openmower-firmware.bin /openmower-firmware.bin
COPY --from=builder /project/build/${BUILD_PRESET}/openmower-firmware.elf /openmower-firmware.elf
