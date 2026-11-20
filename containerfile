
FROM docker.io/library/gcc:16

RUN apt-get update && \
    apt-get install -y \
        cmake \
        gdb \
        ninja-build \
        pkg-config \
        libmnl-dev \
        gcc-aarch64-linux-gnu \
        g++-aarch64-linux-gnu \
        gcc-x86-64-linux-gnu \
        g++-x86-64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
