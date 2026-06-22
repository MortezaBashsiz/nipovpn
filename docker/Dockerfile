FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    libyaml-cpp-dev \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --target nipovpn -j$(nproc)


FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libboost-regex1.83.0 \
    libssl3t64 \
    libyaml-cpp0.8 \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /etc/nipovpn /var/log/nipovpn

COPY --from=builder /src/build/core/nipovpn /usr/local/bin/nipovpn
COPY nipovpn/etc/nipovpn/config.yaml /etc/nipovpn/config.yaml

ENTRYPOINT ["nipovpn"]
CMD ["server", "/etc/nipovpn/config.yaml"]