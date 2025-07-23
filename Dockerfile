FROM ubuntu:22.04 AS builder
LABEL authors="Clemens Elflein"

RUN apt-get update && apt-get install -y  \
    gcc-arm-none-eabi git \
    libasio-dev iproute2 \
    python3 python3-venv python3-pip \
    cmake \
    make \
    && rm -rf /var/lib/apt/lists/*

# Install ccache (TODO: REMOVE as soon as we have universal firmware)
RUN apt-get update && apt-get install -y ccache \
    && rm -rf /var/lib/apt/lists/* \
    && /usr/sbin/update-ccache-symlinks
ENV PATH="/usr/lib/ccache:$PATH"

RUN pip install elf-size-analyze

COPY . /project

WORKDIR /project
RUN mkdir build
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=YardForce -BYardForce && cd YardForce && make -j$(nproc)
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=Worx -BWorx && cd Worx && make -j$(nproc)
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=Lyfco_E1600 -BLyfco_E1600 && cd Lyfco_E1600 && make -j$(nproc)
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=Sabo -BSabo && cd Sabo && make -j$(nproc)
RUN cd build && cmake .. --preset=Release -DROBOT_PLATFORM=xBot -BxBot && cd xBot && make -j$(nproc)
# Use Sabo build for RAM and ROM analysis for now - it has the most libraries
RUN elf-size-analyze -H -R -t arm-none-eabi- ./build/Sabo/openmower.elf -W > build/ram-info.html
RUN elf-size-analyze -H -F -t arm-none-eabi- ./build/Sabo/openmower.elf -W > build/flash-info.html
RUN ccache -s > build/ccache.txt

FROM scratch
COPY --from=builder /project/build/ccache.txt /ccache.txt
COPY --from=builder /project/build/ram-info.html /ram-info.html
COPY --from=builder /project/build/flash-info.html /flash-info.html

COPY --from=builder /project/build/YardForce/openmower.bin /openmower-yardforce.bin
COPY --from=builder /project/build/YardForce/openmower.elf /openmower-yardforce.elf

COPY --from=builder /project/build/Worx/openmower.bin /openmower-worx.bin
COPY --from=builder /project/build/Worx/openmower.elf /openmower-worx.elf

COPY --from=builder /project/build/Lyfco_E1600/openmower.bin /openmower-lyfco-e1600.bin
COPY --from=builder /project/build/Lyfco_E1600/openmower.elf /openmower-lyfco-e1600.elf

COPY --from=builder /project/build/Sabo/openmower.bin /openmower-sabo.bin
COPY --from=builder /project/build/Sabo/openmower.elf /openmower-sabo.elf

COPY --from=builder /project/build/xBot/openmower.bin /openmower-xbot.bin
COPY --from=builder /project/build/xBot/openmower.elf /openmower-xbot.elf
