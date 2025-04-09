FROM ubuntu:22.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi git \
    libasio-dev iproute2 \
    python3 python3-venv python3-pip \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

COPY . /project

WORKDIR /project
RUN mkdir build
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=YardForce -BYardForce && cd YardForce && make -j$(nproc)
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=Worx -BWorx && cd Worx && make -j$(nproc)
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=Lyfco_E1600 -BLyfco_E1600 && cd Lyfco_E1600 && make -j$(nproc)


FROM scratch
COPY --from=builder /project/build/YardForce/openmower.bin /openmower-yardforce.bin
COPY --from=builder /project/build/YardForce/openmower.elf /openmower-yardforce.elf

COPY --from=builder /project/build/Worx/openmower.bin /openmower-worx.bin
COPY --from=builder /project/build/Worx/openmower.elf /openmower-worx.elf

COPY --from=builder /project/build/Lyfco_E1600/openmower.bin /openmower-lyfco-e1600.bin
COPY --from=builder /project/build/Lyfco_E1600/openmower.elf /openmower-lyfco-e1600.elf
