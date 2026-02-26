# MapServer可执行程序
cmake_minimum_required(VERSION 3.15)

# ============== Protocol Buffers ==============
# find_package(protobuf CONFIG REQUIRED)  # Using vcpkg paths directly
# find_package(absl CONFIG REQUIRED)  # Using vcpkg paths directly

# 包含生成的 Protocol Buffers 头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/protocol
)

# 收集 Protocol Buffers 生成的文件
file(GLOB PROTO_SRCS ${CMAKE_SOURCE_DIR}/protocol/*.cc)
file(GLOB PROTO_HDRS ${CMAKE_SOURCE_DIR}/protocol/*.h)

# 显示找到的文件（调试用）
message(STATUS "Protocol Buffers source files:")
foreach(src ${PROTO_SRCS})
    message(STATUS "  ${src}")
endforeach()

# 创建 Protocol Buffers 库
add_library(murim_protocol STATIC ${PROTO_SRCS})
target_link_libraries(murim_protocol PRIVATE "D:/vcpkg/installed/x64-windows/lib/libprotobuf.lib")
target_include_directories(murim_protocol PUBLIC ${CMAKE_SOURCE_DIR}/protocol "D:/vcpkg/installed/x64-windows/include")

# ============== MapServer Sources ==============
set(MAP_SERVER_SOURCES
    main/main.cpp
    managers/MapServerManager.cpp
    managers/EntityManager.cpp
    managers/ServerCommunicationManager.cpp
    managers/SpawnPointManager.cpp
    managers/NPCManager.cpp
    managers/ShopManager.cpp
    managers/DialogueManager.cpp
    managers/WarehouseManager.cpp
    entities/Monster.cpp
    ai/MonsterAI.cpp
    ai/MonsterStateMachine.cpp
)

add_executable(murim_mapserver ${MAP_SERVER_SOURCES})

# 设置C++20标准
target_compile_features(murim_mapserver PUBLIC cxx_std_20)

# 不使用 header-only 模式，链接预编译的 spdlog 库
# target_compile_definitions(murim_mapserver PRIVATE SPDLOG_HEADER_ONLY)

# 包含头文件目录
target_include_directories(murim_mapserver
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../
    "C:/Program Files/PostgreSQL/17/include"
)

# 链接依赖库
target_link_libraries(murim_mapserver
    PRIVATE
    murim_protocol
    "D:/vcpkg/installed/x64-windows/lib/libprotobuf.lib"
    "D:/vcpkg/installed/x64-windows/lib/spdlog.lib"
    # PostgreSQL 库
    "C:/Program Files/PostgreSQL/17/lib/libpq.lib"
    ws2_32
    secur32
    advapi32
    # Abseil 日志库 (手动编译完整版本)
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_message.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_check_op.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_entry.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_initialize.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_flags.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_globals.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_sink.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_vlog_config_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_format.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_globals.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_log_sink_set.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_proto.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_structured_proto.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_nullguard.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_conditions.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_internal_fnmatch.lib"
    # Abseil 基础库
    "D:/vcpkg/installed/x64-windows/lib/absl_base.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_raw_logging_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_log_severity.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_strings.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_string_view.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_strings_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_str_format_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_synchronization.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_time.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_time_zone.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_civil_time.lib"
    # TODO: absl_flags.lib 不存在，已移除
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_config.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_commandlineflag.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_parse.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_usage.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_marshalling.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_reflection.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_private_handle_accessor.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_program_name.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_flags_usage_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_spinlock_wait.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_int128.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_city.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_hash.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_status.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_statusor.lib"
    # 额外需要的 abseil 库
    "D:/vcpkg/installed/x64-windows/lib/absl_throw_delegate.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_strerror.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_malloc_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_leak_check.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_graphcycles_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_kernel_timeout_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_stacktrace.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_symbolize.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_debugging_internal.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_examine_stack.lib"
    "D:/vcpkg/installed/x64-windows/lib/absl_failure_signal_handler.lib"
    # TODO: absl_flags.lib 不存在，已移除
    murim_network
    murim_core
    murim_resource
    murim_localization
    murim_spatial_lib
    murim_shared
    murim_utils
    murim_game
    murim_character
    murim_skill
    murim_item
    murim_quest
    murim_ai
    murim_social
    murim_battle
    murim_movement
    murim_npc
    murim_shop
    murim_warehouse
    murim_loot
    murim_mail
    murim_market
    murim_trade
    murim_leaderboard
    murim_achievement
    murim_title
    murim_vip
    murim_dungeon
    murim_signin
    murim_marriage
    # TODO: API 不匹配需要重构
    # murim_pet
    # murim_mount
)

# Windows特定设置
if(WIN32)
    target_compile_definitions(murim_mapserver PRIVATE
        _WIN32_WINNT=0x0601
    )
    target_link_options(murim_mapserver PRIVATE
        /LIBPATH:D:/vcpkg/installed/x64-windows/lib
        /LIBPATH:C:/Program Files/PostgreSQL/17/lib
        # TODO: Abseil 库在 vcpkg 中不完整，暂时移除
        # /WHOLEARCHIVE:absl_log_flags.lib
        # /WHOLEARCHIVE:absl_log_internal_structured_proto.lib
        # /WHOLEARCHIVE:absl_flags_config.lib
        # /WHOLEARCHIVE:absl_flags_internal.lib
        # /WHOLEARCHIVE:absl_flags_commandlineflag.lib
        # /WHOLEARCHIVE:absl_flags_parse.lib
        # /WHOLEARCHIVE:absl_flags_usage.lib
        # /WHOLEARCHIVE:absl_poison.lib
    )
endif()

# 安装规则
install(TARGETS murim_mapserver
    RUNTIME DESTINATION bin
)
