# 协议层库
add_library(murim_protocol STATIC
    MessageRegistry.cpp
    NetworkPacket.cpp
)

target_include_directories(murim_protocol
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        D:/vcpkg/installed/x64-windows/include
        ${GENERATED_PROTOBUF_PATH}
)

target_link_libraries(murim_protocol
    PUBLIC

)

# 如果 Protobuf 可用，添加链接
if(Protobuf_FOUND)
    target_link_libraries(murim_protocol
        PUBLIC
            protobuf::libprotobuf
    )
endif()

# ============== 安装 ==============
install(TARGETS murim_protocol
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
