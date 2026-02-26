# 社交系统库
# # spdlog is used in header-only mode (SPDLOG_HEADER_ONLY)
# No need to find_package for header-only usage

add_library(murim_social STATIC
    FriendManager.cpp
    Party.cpp
    PartyManager.cpp
    ChatManager.cpp
    ChatHandler.cpp
)

target_include_directories(murim_social
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_social
    PUBLIC
        murim_database
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_social PUBLIC cxx_std_20)
