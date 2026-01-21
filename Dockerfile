FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    netcat \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code
COPY . .

# Build and Install to Global Path (Safe from Volume Mounts)
RUN rm -rf build && mkdir build && cd build && cmake .. && make \
    && cp server /usr/local/bin/server \
    && cp lb /usr/local/bin/lb

# Default command
CMD ["server"]