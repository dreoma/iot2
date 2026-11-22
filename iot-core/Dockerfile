FROM ubuntu:24.04 AS build
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    cmake g++ libc-ares-dev libbrotli-dev libmariadb-dev libmariadb-dev-compat libcurl4-openssl-dev libdrogon-dev libhiredis-dev libjsoncpp-dev libpq-dev libsqlite3-dev libssl-dev libyaml-cpp-dev uuid-dev zlib1g-dev
WORKDIR /app/iot-core
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    ca-certificates git libboost-system-dev nlohmann-json3-dev libsodium-dev pkg-config
# Зафиксированная версия официальной библиотеки tgbot-cpp.
RUN git init /tmp/tgbot-cpp \
 && git -C /tmp/tgbot-cpp remote add origin https://github.com/reo7sp/tgbot-cpp.git \
 && git -C /tmp/tgbot-cpp fetch --depth 1 origin 6a0591c3e821dc483818bdca4ef5b17733d62f21 \
 && git -C /tmp/tgbot-cpp checkout FETCH_HEAD \
 && cmake -S /tmp/tgbot-cpp -B /tmp/tgbot-cpp/build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build /tmp/tgbot-cpp/build -j2 \
 && cmake --install /tmp/tgbot-cpp/build
COPY . /app
RUN cmake -S . -B build && cmake --build build -j2

FROM ubuntu:24.04
RUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    ca-certificates libcurl4 libboost-system-dev libdrogon-dev libpq5 libsodium23 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app/iot-core/build/iot-core ./iot-core
COPY --from=build /usr/local/lib/ /usr/local/lib/
RUN ldconfig
COPY iot-core/config.docker.json ./config.json
CMD ["./iot-core"]
