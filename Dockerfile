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
RUN mkdir build

# Share one FetchContent download/source dir so deps are fetched once.
ENV FETCHCONTENT_BASE_DIR=/project/build/_deps
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -B${BUILD_PRESET} && cd ${BUILD_PRESET} && make -j$(nproc)
RUN elf-size-analyze -H -R -t arm-none-eabi- ./build/${BUILD_PRESET}/openmower.elf -W > build/ram-info.html
RUN elf-size-analyze -H -F -t arm-none-eabi- ./build/${BUILD_PRESET}/openmower.elf -W > build/flash-info.html

FROM scratch
ARG BUILD_PRESET=Release
COPY --from=builder /project/build/ram-info.html /ram-info.html
COPY --from=builder /project/build/flash-info.html /flash-info.html
COPY --from=builder /project/build/${BUILD_PRESET}/openmower.bin /openmower.bin
COPY --from=builder /project/build/${BUILD_PRESET}/openmower.elf /openmower.elf
