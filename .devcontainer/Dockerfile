FROM ubuntu:22.04

ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME && \
    useradd --uid $USER_UID --gid $USER_GID --groups sudo --password '' -s /bin/bash -m $USERNAME

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean && \
    apt-get update && \
    apt-get install --no-install-recommends --yes \
    sudo git less \
    gcc gcc-arm-none-eabi libstdc++-arm-none-eabi-newlib  \
    libasio-dev iproute2 \
    python3 python3-venv python3-pip \
    cmake make \
    gdb-multiarch && \
    ln -s /usr/bin/nm /usr/bin/nm-multiarch && \
    ln -s /usr/bin/objdump /usr/bin/objdump-multiarch && \
    echo 'source /usr/share/bash-completion/completions/git' >> /home/dev/.bashrc

USER $USERNAME
