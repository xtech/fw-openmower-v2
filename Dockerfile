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
# Each platform builds in its own dir (build/<platform>) with a different CWD.
# Without these, ccache hashes the absolute paths in the compiler args and the
# working directory, so identical dependency sources (ChibiOS, lvgl, ...) miss
# the cache on every platform. BASEDIR rewrites abs paths under /project to
# relative; NOHASHDIR drops the CWD from the hash.
ENV CCACHE_BASEDIR="/project"
ENV CCACHE_NOHASHDIR="true"

RUN pip install elf-size-analyze

# Build preset to use for all platforms. CI passes "Debug" for pull requests and
# "Release" for main / tagged releases.
ARG BUILD_PRESET=Release

COPY . /project

WORKDIR /project
RUN mkdir build

# Share one FetchContent download/source dir across all platform configures so
# deps like lwjson are fetched from GitHub once instead of once per platform
# (avoids tripping codeload's transient 500s / rate limiting).
ENV FETCHCONTENT_BASE_DIR=/project/build/_deps
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=YardForce -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BYardForce && cd YardForce && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=YardForce_V4 -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BYardForce_V4 && cd YardForce_V4 && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=Worx -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BWorx && cd Worx && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=Lyfco_E1600 -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BLyfco_E1600 && cd Lyfco_E1600 && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=Sabo -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BSabo && cd Sabo && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=xBot -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BxBot && cd xBot && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=Universal5S -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BUniversal5S && cd Universal5S && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=Universal7S -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BUniversal7S && cd Universal7S && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=Universal8S -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -BUniversal8S && cd Universal8S && make -j$(nproc)
RUN cd build && cmake .. --preset=${BUILD_PRESET} -DROBOT_PLATFORM=husq310MKII -DFETCHCONTENT_BASE_DIR=${FETCHCONTENT_BASE_DIR} -Bhusq310MKII && cd husq310MKII && make -j$(nproc)

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

COPY --from=builder /project/build/YardForce_V4/openmower.bin /openmower-yardforce-v4.bin
COPY --from=builder /project/build/YardForce_V4/openmower.elf /openmower-yardforce-v4.elf

COPY --from=builder /project/build/Worx/openmower.bin /openmower-worx.bin
COPY --from=builder /project/build/Worx/openmower.elf /openmower-worx.elf

COPY --from=builder /project/build/Lyfco_E1600/openmower.bin /openmower-lyfco-e1600.bin
COPY --from=builder /project/build/Lyfco_E1600/openmower.elf /openmower-lyfco-e1600.elf

COPY --from=builder /project/build/Sabo/openmower.bin /openmower-sabo.bin
COPY --from=builder /project/build/Sabo/openmower.elf /openmower-sabo.elf

COPY --from=builder /project/build/xBot/openmower.bin /openmower-xbot.bin
COPY --from=builder /project/build/xBot/openmower.elf /openmower-xbot.elf

COPY --from=builder /project/build/Universal5S/openmower.bin /openmower-universal-5s.bin
COPY --from=builder /project/build/Universal5S/openmower.elf /openmower-universal-5s.elf

COPY --from=builder /project/build/Universal7S/openmower.bin /openmower-universal-7s.bin
COPY --from=builder /project/build/Universal7S/openmower.elf /openmower-universal-7s.elf

COPY --from=builder /project/build/Universal8S/openmower.bin /openmower-universal-8s.bin
COPY --from=builder /project/build/Universal8S/openmower.elf /openmower-universal-8s.elf

COPY --from=builder /project/build/husq310MKII/openmower.bin /openmower-husq310MKII.bin
COPY --from=builder /project/build/husq310MKII/openmower.elf /openmower-husq310MKII.elf
