# 工具类库
# # spdlog is used in header-only mode (SPDLOG_HEADER_ONLY)
# No need to find_package for header-only usage

add_library(murim_utils STATIC
    TimeUtils.cpp
)

target_include_directories(murim_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(murim_utils
    PUBLIC
        murim_core
        # spdlog::spdlog  # Using header-only spdlog
)

target_compile_features(murim_utils PUBLIC cxx_std_20)
