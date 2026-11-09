# nheqminer Dockerfile
# Equihash CPU miner for Zcash mining pools

FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libboost-system-dev \
    libsodium-dev \
    libboost-log-dev \
    libboost-date-time-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    fasm \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy source to /src/nheqminer (CMakeLists.txt expects build dir as sibling)
WORKDIR /src/nheqminer
COPY . .

# Assemble xenoncat AVX objects using system fasm
WORKDIR /src/nheqminer/cpu_xenoncat/asm_linux
RUN fasm -m 1280000 equihash_avx1.asm && fasm -m 1280000 equihash_avx2.asm

# Build nheqminer (CPU only, no CUDA in container)
# Build directory must be sibling to source for CMakeLists.txt paths to work
WORKDIR /src/build
RUN cmake ../nheqminer \
    -DUSE_CPU_XENONCAT=ON \
    -DUSE_CPU_TROMP=OFF \
    -DUSE_CPU_GENERIC=ON \
    -DUSE_CUDA_DJEZO=OFF \
    -DUSE_CUDA_TROMP=OFF \
    && make -j$(nproc)

# Runtime image
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libsodium23 \
    libboost-log1.74.0 \
    libboost-date-time1.74.0 \
    libboost-filesystem1.74.0 \
    libboost-thread1.74.0 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/nheqminer /usr/local/bin/

RUN useradd --create-home --shell /bin/false miner
USER miner

# Default: run with help to show usage
ENTRYPOINT ["nheqminer"]
CMD ["-h"]
