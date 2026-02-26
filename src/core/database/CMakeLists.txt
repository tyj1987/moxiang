# Database layer library
add_library(murim_database STATIC
    DatabasePool.cpp
    QueryExecutor.cpp
    DatabaseUtils.cpp
    CharacterDAO.cpp
    ItemDAO.cpp
    AccountDAO.cpp
    SkillDAO.cpp
    QuestDAO.cpp
    FriendDAO.cpp
    PartyDAO.cpp
    ChatDAO.cpp
    GuildDAO.cpp
    MonsterDAO.cpp
    NPCDAO.cpp
    ShopDAO.cpp
    DialogueDAO.cpp
    WarehouseDAO.cpp
)

target_include_directories(murim_database
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}${CMAKE_SOURCE_DIR}
)

# 查找依赖包
# find_package(spdlog CONFIG REQUIRED)  # Using header-only from vcpkg

# Windows特定的预处理器定义
if(WIN32)
    target_compile_definitions(murim_database PRIVATE
        NOMINMAX                  # 避免Windows宏冲突
        WIN32_LEAN_AND_MEAN       # 减少Windows头文件包含
        _CRT_SECURE_NO_WARNINGS    # 禁用CRT安全警告
        _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS  # 禁用MS扩展警告
    )
endif()

# 使用系统安装的 PostgreSQL 18
if(WIN32)
    target_include_directories(murim_database
        PUBLIC
            "C:/Program Files/PostgreSQL/17/include"
    )
endif()

# ============================================
# 链接 PostgreSQL libpq 和 spdlog
# ============================================
target_link_libraries(murim_database
    PUBLIC
        "C:/Program Files/PostgreSQL/17/lib/libpq.lib"  # 使用系统安装的libpq
        ws2_32                    # Windows sockets
        secur32                   # Windows security
        advapi32                  # Windows crypto API (for SHA-256)
        murim_core
)

target_compile_features(murim_database PUBLIC cxx_std_20)
