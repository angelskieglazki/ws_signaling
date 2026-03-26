# CMakeLists.txt для Qt6 WebSocket клиента

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Локальные настройки (путь к Qt, компилятор и т.д.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/local.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/local.cmake")
else()
    message(WARNING "local.cmake not found. Copy local.cmake.example to local.cmake and set your Qt path.")
endif()

find_package(Qt6 COMPONENTS Core Gui Widgets WebSockets Multimedia REQUIRED)

# Ищем libdatachannel (установлен из исходников в /usr/local)
find_library(LIBDATACHANNEL_LIBRARY
    NAMES datachannel
    PATHS /usr/local/lib
)

find_path(LIBDATACHANNEL_INCLUDE_DIR
    NAMES rtc/rtc.hpp
    PATHS /usr/local/include
)

if(NOT LIBDATACHANNEL_LIBRARY)
    message(FATAL_ERROR "libdatachannel not found. Install: cd libdatachannel && mkdir build && cd build && cmake .. && make && sudo make install")
endif()

message(STATUS "Found libdatachannel: ${LIBDATACHANNEL_LIBRARY}")
message(STATUS "Found libdatachannel includes: ${LIBDATACHANNEL_INCLUDE_DIR}")

add_executable(qt-client
    client.cpp
    audiocapture.cpp
    audiocapture.h
    webrtcmanager.cpp
    webrtcmanager.h
)

target_include_directories(qt-client PRIVATE
    ${LIBDATACHANNEL_INCLUDE_DIR}
)

target_link_libraries(qt-client
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebSockets
    Qt6::Multimedia
    ${LIBDATACHANNEL_LIBRARY}
)

# libdatachannel зависит от OpenSSL и pthread
target_link_libraries(qt-client
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)
