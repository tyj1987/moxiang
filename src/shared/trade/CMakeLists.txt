cmake_minimum_required(VERSION 3.15)

# 交易系统库
# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

add_library(murim_trade STATIC
    TradeManager.cpp
    TradeHandler.cpp
)

target_include_directories(murim_trade
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_trade
    PUBLIC
        murim_database
        murim_core
        # spdlog::spdlog_header_only  # Using header-only from vcpkg
)

target_compile_features(murim_trade PUBLIC cxx_std_17)

# ============== 安装 ==============
install(TARGETS murim_trade
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
