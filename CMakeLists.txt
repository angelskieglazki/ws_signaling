cmake_minimum_required(VERSION 3.15)
project(webrtc-signaling LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Общие пути к зависимостям
set(WEBSOCKETPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/websocketpp" CACHE PATH "Path to websocketpp")
set(ASIO_ROOT "/usr/include" CACHE PATH "Path to standalone asio")

include_directories(
    ${WEBSOCKETPP_ROOT}
    ${ASIO_ROOT}
)

find_package(OpenSSL REQUIRED)

# Подключаем подпроекты
add_subdirectory(server)
add_subdirectory(client)
