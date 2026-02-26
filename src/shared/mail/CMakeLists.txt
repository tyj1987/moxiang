cmake_minimum_required(VERSION 3.15)

# 邮件系统库
# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

add_library(murim_mail STATIC
    MailManager.cpp
    MailHandler.cpp
)

target_include_directories(murim_mail
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party/libpq/include
)

target_link_libraries(murim_mail
    PUBLIC
        murim_database
        murim_core
)

target_compile_features(murim_mail PUBLIC cxx_std_17)
