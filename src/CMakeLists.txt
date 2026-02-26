# ============== 核心库 ==============
add_subdirectory(core)

# ============== 共享游戏逻辑库 ==============
add_subdirectory(shared)

# ============== 服务器可执行文件 ==============
add_subdirectory(map-server)
add_subdirectory(agent-server)
add_subdirectory(distribute-server)
add_subdirectory(murimnet-server)
