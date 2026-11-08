cmake_minimum_required(VERSION 3.20)
project(http_adapter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(http-adapter
    src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/crypto/encoder.cpp
)
target_include_directories(http-adapter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(http-adapter PRIVATE Drogon::Drogon CURL::libcurl OpenSSL::Crypto)
