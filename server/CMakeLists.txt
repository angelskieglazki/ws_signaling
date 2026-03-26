# CMakeLists.txt для signaling-server

add_executable(signaling-server server.cpp)

target_link_libraries(signaling-server
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

target_compile_definitions(signaling-server PRIVATE
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    ASIO_STANDALONE
    ASIO_HAS_STD_CHRONO
)
