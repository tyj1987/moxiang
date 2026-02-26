# 账号系统库
# find_package(spdlog REQUIRED)
# find_package(OpenSSL REQUIRED)  # Not needed, using Windows crypto APIs

add_library(murim_account STATIC
    LoginManager.cpp
)

target_include_directories(murim_account
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
)

# 链接依赖
target_link_libraries(murim_account
    PUBLIC
        murim_database
        murim_security
        advapi32  # Windows crypto API (SHA-256)
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_account PUBLIC cxx_std_20)
