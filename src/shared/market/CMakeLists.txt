cmake_minimum_required(VERSION 3.15)

# 市场系统库
add_library(murim_market STATIC
    MarketManager.cpp
    MarketHandler.cpp
)

target_include_directories(murim_market
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_market
    PUBLIC
        murim_mail
)

target_compile_features(murim_market PUBLIC cxx_std_17)

# ============== 安装 ==============
install(TARGETS murim_market
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
