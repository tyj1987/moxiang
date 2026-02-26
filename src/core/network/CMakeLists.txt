# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

add_library(murim_network STATIC
    IOContext.cpp
    Connection.cpp
    Session.cpp
    Acceptor.cpp
    SessionManager.cpp
    ConnectionManager.cpp
    PacketSerializer.cpp
    ByteBuffer.cpp
    ServerMessage.cpp
    ServerConnection.cpp
    NotificationService.cpp
)

# Boost.Asio - use vcpkg include path for header-only components
# find_package(Boost REQUIRED)  # Not needed, using vcpkg paths directly

target_link_libraries(murim_network
    PUBLIC
        murim_core
        ws2_32  # Windows Socket库
)

target_include_directories(murim_network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        D:/vcpkg/installed/x64-windows/include
)

target_compile_features(murim_network PUBLIC cxx_std_17)

# ============== 安装 ==============
install(TARGETS murim_network
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
