# ============== Session 管理层 ==============
find_package(spdlog CONFIG REQUIRED)

add_library(murim_session STATIC
    SessionManager.cpp
)

target_link_libraries(murim_session
    PUBLIC
        murim_core
)

target_include_directories(murim_session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(murim_session PUBLIC cxx_std_20)
