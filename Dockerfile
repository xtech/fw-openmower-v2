FROM ubuntu:22.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi  \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

COPY . /project

WORKDIR /project
RUN mkdir build && cd build && cmake .. --preset=Release && cd Release && make -j$(nproc)


FROM scratch
COPY --from=builder /project/build/Release/openmower-with-unsafe-bootloader.bin /
COPY --from=builder /project/build/Release/openmower.bin /
COPY --from=builder /project/build/Release/openmower.elf /
