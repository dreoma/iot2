cmake_minimum_required(VERSION 3.20)
project(iot_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Установите Drogon через vcpkg или пакетный менеджер системы.
find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_library(TGBOT_LIB TgBot REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_executable(iot-core
    src/main.cpp
    src/routes/user_routes.cpp
    src/routes/device_routes.cpp
    src/routes/rule_routes.cpp
    src/services/auth.cpp
    src/repositories/rule_repository.cpp
    src/services/rule_engine.cpp
    src/services/NotificationService.cpp
    src/notifications/TelegramNotifier.cpp
    src/notifications/EmailNotifier.cpp
    src/repositories/user_repository.cpp
    src/repositories/device_repository.cpp
    src/repositories/telemetry_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/crypto/decoder.cpp
)
target_include_directories(iot-core PRIVATE src)
target_include_directories(iot-core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_link_libraries(iot-core PRIVATE Drogon::Drogon ${TGBOT_LIB}
    CURL::libcurl OpenSSL::SSL OpenSSL::Crypto Boost::system PkgConfig::SODIUM)
