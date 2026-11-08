FROM ubuntu:24.04 AS build
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    cmake g++ libc-ares-dev libbrotli-dev libmariadb-dev libmariadb-dev-compat libcurl4-openssl-dev libdrogon-dev libhiredis-dev libjsoncpp-dev libpq-dev libsqlite3-dev libssl-dev libyaml-cpp-dev uuid-dev zlib1g-dev
WORKDIR /app/http-adapter
COPY . /app
RUN cmake -S . -B build && cmake --build build -j2

FROM ubuntu:24.04
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    libcurl4 libdrogon-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app/http-adapter/build/http-adapter ./http-adapter
COPY http-adapter/config.json ./config.json
CMD ["./http-adapter"]
