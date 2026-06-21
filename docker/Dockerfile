FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    make \
    python3 \
    python3-pip \
    ninja-build \
    iproute2 \
    libboost-all-dev \
    libbsd-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

RUN git clone https://github.com/simongog/sdsl-lite.git && \
    cd sdsl-lite && \
    ./install.sh /usr/local

WORKDIR /RingOA

COPY . .

RUN python3 build.py --setup
RUN python3 build.py

CMD ["/bin/bash"]
