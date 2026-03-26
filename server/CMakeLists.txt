# CMakeLists.txt для signaling-server

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ищем nlohmann/json (header-only)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    # Если не найден через CMake, ищем в системе
    find_path(NLOHMANN_JSON_INCLUDE_DIR
        NAMES nlohmann/json.hpp
        PATHS /usr/include /usr/local/include
    )
    if(NOT NLOHMANN_JSON_INCLUDE_DIR)
        message(FATAL_ERROR "nlohmann/json not found. Install: sudo apt install nlohmann-json3-dev")
    endif()
endif()

add_executable(signaling-server server.cpp)

target_link_libraries(signaling-server
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    $<IF:$<TARGET_EXISTS:nlohmann_json::nlohmann_json>,nlohmann_json::nlohmann_json,>
)

if(NLOHMANN_JSON_INCLUDE_DIR)
    target_include_directories(signaling-server PRIVATE ${NLOHMANN_JSON_INCLUDE_DIR})
endif()

target_compile_definitions(signaling-server PRIVATE
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    ASIO_STANDALONE
    ASIO_HAS_STD_CHRONO
)
