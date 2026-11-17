# docker build -t macaronica .
# docker run --rm macaronica      roda o make test
FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        clang-18 llvm-18 mlir-18-tools libomp-18-dev \
        gcc-avr avr-libc binutils-avr avrdude simavr \
        python3 python3-matplotlib \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /projeto
COPY . /projeto
RUN make clean && make

CMD ["make", "test"]
