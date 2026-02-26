cmake_minimum_required(VERSION 3.15)
project(MurimServer VERSION 1.0.0 LANGUAGES CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============== 核心库 ==============
add_subdirectory(core)

# ============== 共享游戏逻辑库 ==============
add_subdirectory(shared)

# ============== 服务器可执行文件 ==============
add_subdirectory(map-server)
add_subdirectory(agent-server)
add_subdirectory(distribute-server)
add_subdirectory(murimnet-server)
