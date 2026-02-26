# ============== 核心库 ==============
add_library(murim_core INTERFACE)

target_include_directories(murim_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}  # For core/xxx.hpp access from any module
    D:/vcpkg/installed/x64-windows/include  # vcpkg include directory for spdlog, fmt, etc.
    "C:/Program Files/PostgreSQL/17/include"  # PostgreSQL headers
)

# 链接 spdlog（header-only，仅需要include目录）
# # find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg  # Not needed, using header-only from vcpkg
# # find_package(nlohmann_json CONFIG REQUIRED)  # Using header-only from vcpkg  # Not needed, using header-only from vcpkg
target_compile_definitions(murim_core INTERFACE SPDLOG_HEADER_ONLY)

# fmt库需要显式链接（spdlog依赖）
target_link_libraries(murim_core INTERFACE "D:/vcpkg/installed/x64-windows/lib/fmt.lib")

# 定义编译选项
target_compile_features(murim_core INTERFACE cxx_std_20)

# ============== 网络层 ==============
add_subdirectory(network)

# ============== 数据库层 ==============
add_subdirectory(database)

# ============== 空间索引层 ==============
add_subdirectory(spatial)

# ============== 本地化层 ==============
add_subdirectory(localization)

# ============== 安全层 ==============
add_subdirectory(security)

# ============== Session 管理层 ==============
add_subdirectory(session)

# ============== 资源管理层 ==============
add_subdirectory(resource)
