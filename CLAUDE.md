# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 提供在此代码库中工作的指导。

## 🔧 开发环境要求

### 服务端（new-server/）

**必需工具**：
- **编译器**：
  - Visual Studio 2017 或更高版本（推荐 VS2026）
  - VS2026 (18.3.0) 已完全支持
  - 或 GCC 7+（Linux）
  - 或 Clang 5+（macOS/Linux）

- **构建系统**：
  - CMake 3.15+（当前环境：4.2.0）
  - Ninja（可选，用于更快的构建）

- **数据库**：
  - PostgreSQL 15+（推荐 15.x 或 16.x）
  - psql 客户端工具

- **依赖库**（Windows vcpkg）：
  - boost-asio（网络）
  - libpq（PostgreSQL 客户端）
  - protobuf（协议缓冲区）

**C++ 标准**：
- **C++17**（在 CMakeLists.txt 中明确设置）
- 支持完整的 C++17 标准库特性
- 不使用 C++20 特性（保持最大兼容性）

**当前环境配置**（D:\moxiang\new-server\CMakeLists.txt）：
```cmake
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

### 客户端（new-client/）

**必需工具**：
- **Unreal Engine**：
  - Unreal Engine 5.7（在 MurimClient.uproject 中明确设置）
  - 从 Epic Games Launcher 安装

- **编译器**：
  - Visual Studio 2019+（推荐 VS2026）
  - VS2026 (18.x) 完全支持 UE5.7
  - 必需组件："使用 C++ 的游戏开发"
  - Windows 10/11 SDK

- **平台**：
  - Windows 10 64-bit 或更高
  - DirectX 11/12 支持

**当前环境配置**（D:\moxiang\new-client\MurimClient.uproject）：
```json
{
  "EngineAssociation": "5.7",
  "TargetPlatforms": ["Windows"]
}
```

**启用的 UE5 插件**：
- EnhancedInput（增强输入系统）
- OnlineSubsystem（在线子系统）
- OnlineSubsystemUtils（在线子系统工具）

### 工具链（new-tools/）

**必需工具**：
- **Python**：
  - Python 3.11+
  - 用于数据转换脚本

- **Git**：
  - Git 2.30+
  - 用于版本控制

### 推荐开发环境配置

**Windows 完整开发环境**：
```batch
# 1. Visual Studio 2026 Community/Professional/Enterprise (推荐)
#    版本：18.3.0+ (2026年2月功能更新)
#    安装组件：
#    - 使用 C++ 的游戏开发 ✓
#    - Windows 10/11 SDK
#    - CMake 工具（可选）

# 2. Unreal Engine 5.7
#    从 Epic Games Launcher 安装

# 3. PostgreSQL 15.x/16.x
#    下载：https://www.postgresql.org/download/windows/
#    安装时记住密码

# 4. Git
#    下载：https://git-scm.com/download/win

# 5. CMake（如果未使用 VS 内置的）
#    下载：https://cmake.org/download/

# 6. vcpkg（可选，用于依赖管理）
#    git clone https://github.com/Microsoft/vcpkg.git
#    .\vcpkg\bootstrap-vcpkg.bat
#    .\vcpkg integrate install
```

### 版本兼容性矩阵

| 工具 | 最低版本 | 推荐版本 | 当前测试环境 |
|------|----------|----------|----------|
| Visual Studio | 2017 (MSVC 19.14) | 2026 (18.3+) | 2026 (18.3.0) ✅ |
| CMake | 3.15 | 3.25+ | 4.2.0 ✅ |
| PostgreSQL | 15.0 | 15.x/16.x | 需安装 |
| Unreal Engine | 5.7 | 5.7 | 5.7 ✅ |
| Python | 3.11 | 3.11+ | - |
| C++ 标准 | C++17 | C++17 | C++17 ✅ |

**Visual Studio 版本说明**：
- **VS2017** (15.x) - 最低支持版本
- **VS2019** (16.x) - 稳定支持
- **VS2022** (17.x) - 稳定支持
- **VS2026** (18.x) - **推荐版本**，完全支持 ✅
  - 18.3.0+ (2026年2月功能更新)
  - Stable 渠道（不是预览版）
  - 已验证 UE5.7 完全兼容
  - 当前环境：18.3.0.11505 ✅

### 环境验证

**验证服务端环境**：
```batch
# 检查 CMake
cmake --version

# 检查编译器（Visual Studio）
cl

# 检查 PostgreSQL
psql --version

# 检查 Git
git --version
```

**验证客户端环境**：
```batch
# 检查 UE5
dir "%EPC_GAME_DIR%\UE_5.7\Engine\Binaries\Win64\UE5Editor.exe"

# 检查项目配置
type new-client\MurimClient.uproject

# 诊断 UE5 问题
new-client\diagnose_ue5.bat
```

## 🚀 快速参考

### 最常用命令

**服务端（Windows + MSVC）**：
```batch
# 构建服务端（从项目根目录）
cd new-server
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 运行服务器
cd ..\..
start_agentserver.bat                    # 启动 AgentServer (端口 7000)
# DistributeServer、MapServer、MurimNetServer 同理

# 数据库初始化
cd new-server\scripts
psql -U postgres -f init_database_complete.sql
```

**客户端（UE5.7 + Windows）**：
```batch
# 打开项目
双击 new-client\MurimClient.uproject

# 编译客户端
cd new-client
build_now.bat

# 资产导入
cd new-client\Scripts
one_click_import.bat
```

**快速故障排除**：
```batch
# 检查服务器状态
new-server\check_status.bat

# 诊断UE5问题
new-client\diagnose_ue5.bat

# 查看日志
type new-server\logs\agent_server.log
type new-client\Saved\Logs\MurimClient.log

# 验证开发环境
cmake --version          # 检查 CMake
cl                      # 检查 Visual Studio
psql --version          # 检查 PostgreSQL（需安装）
```

### 项目概述

**武林外传 (Moxiang - Legend of Martial Arts)** 是一款韩式商业MMORPG的完整1:1复刻项目。本项目将传统的 DirectX 9 / SQL Server 代码库现代化为 Unreal Engine 5.7 / PostgreSQL，同时保持100%功能一致性。

### 核心要求
- **1:1功能一致性**：必须实现原版代码的每一个函数、机制和系统
- **完整资源迁移**：所有资产（模型、动画、纹理、音效）必须完整保留
- **仅中文界面**：所有面向用户的文本、UI和交互必须使用中文
- **可维护架构**：清晰的结构、全面的测试、完善的文档

## 项目结构

```
moxiang/
├── legacy-source-code/          # 原始实现（仅供参考）
│   ├── [Server]Map/             # 旧版MapServer (MSVC 2008)
│   ├── [Server]Agent/           # 旧版AgentServer
│   ├── [Server]Distribute/      # 旧版DistributeServer
│   ├── [Client]MH/              # 旧版DirectX 9客户端
│   ├── [CC]*/                   # 共享的旧版代码
│   └── [Lib]*/                  # 旧版库
│
├── new-server/                  # 现代C++17服务端（主要开发）
│   ├── src/
│   │   ├── core/                # 核心基础设施
│   │   ├── shared/              # 游戏逻辑（跨服务器共享）
│   │   ├── map-server/          # MapServer可执行程序
│   │   ├── agent-server/        # AgentServer可执行程序
│   │   ├── distribute-server/   # DistributeServer可执行程序
│   │   └── murimnet-server/     # MurimNetServer可执行程序
│   ├── proto/                   # Protocol Buffers定义
│   ├── config/                  # JSON配置文件
│   ├── scripts/                 # 数据库初始化脚本
│   └── tests/                   # 单元/集成测试
│
├── new-client/                  # UE5.7客户端（主要开发）
│   ├── Source/                  # C++游戏逻辑
│   │   ├── Network/             # 客户端网络层
│   │   ├── GameLogic/           # 游戏系统
│   │   └── UI/                  # UI实现
│   ├── Content/                 # UE5资产
│   └── Scripts/                 # 资产导入自动化
│
└── new-tools/                   # 现代化和分析工具
    ├── code_analyzer/           # 旧版代码分析
    ├── protocol_converter/      # 二进制 → Protobuf转换器
    └── database_converter/      # SQL Server → PostgreSQL转换器
```

## 架构概览

### 多服务器分布式架构

游戏使用四种专门服务器的微服务架构：

**1. AgentServer (端口 7000)**
- 用户认证和登录
- 会话管理
- 角色创建/删除/列表
- 作为客户端的连接代理

**2. DistributeServer (端口 8000)**
- MapServer的负载均衡
- 将玩家路由到负载最轻的服务器
- 管理集群状态

**3. MapServer (端口 9001+)**
- 游戏世界模拟（每个地图/频道一个实例）
- 战斗、AI、技能、物品、任务
- 100ms刻度率，每个实例最多1000玩家
- 可同时运行多个实例

**4. MurimNetServer (端口 8500)**
- 跨服务器服务
- 帮派聊天、好友系统、市场、邮件

### 服务器通信流程

```
客户端 → AgentServer (登录)
       → DistributeServer (选择MapServer)
       → MapServer (游戏玩法)
            ├─→ MurimNetServer (帮派聊天、市场)
            └─→ PostgreSQL (持久化)
```

### 关键设计模式

**DAO (数据访问对象) 模式**：
- 所有数据库访问通过 `*DAO` 类（AccountDAO, CharacterDAO, ItemDAO等）
- 位于 `src/core/database/dao/`
- 通过 `DatabasePool` 连接池管理

**管理器模式**：
- 游戏系统使用 `*Manager` 类（CharacterManager, ItemManager, SkillManager等）
- 位于 `src/shared/<system>/`
- 单例实例，生命周期管理

**工厂模式**：
- 对象创建通过工厂（GameObjectFactory, ItemFactory等）
- 集中化创建逻辑

**基于组件的实体**：
- `GameObject` 基类具有17个核心战斗属性
- 专门类型：角色、怪物、NPC、物品

## 构建和开发命令

### 服务端开发

**前置要求**：
- C++17编译器（MSVC 2017+, GCC 7+, Clang 5+）
- CMake 3.15+
- PostgreSQL 15+
- vcpkg（Windows上）

**构建命令**：
```bash
cd new-server

# 配置构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建所有服务器
cmake --build . --config Release

# 构建特定服务器
cmake --build . --target murim_mapserver
cmake --build . --target murim_agentserver
cmake --build . --target murim_distributeserver

# 运行测试
ctest --output-on-failure
```

### Windows 平台特定说明

**使用 Visual Studio (推荐)**：
```batch
# 1. 打开 "Developer Command Prompt for VS 2026"
#    或 "x64 Native Tools Command Prompt for VS 2026"

# 2. 进入项目目录
cd D:\moxiang\new-server

# 3. 创建构建目录
mkdir build
cd build

# 4. 生成 Visual Studio 解决方案
cmake .. -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=Release

# 5. 打开解决方案（可选）
start MurimServer.sln

# 6. 或直接命令行构建
cmake --build . --config Release

# 7. 运行所有测试
ctest -C Release --output-on-failure
```

**使用 MinGW/MSYS2**：
```bash
# 在 MSYS2 MinGW 64-bit 终端中
cd new-server
mkdir build && cd build
cmake .. -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**快速启动脚本（Windows）**：
```batch
# 启动单个服务器
new-server\start_agentserver.bat          # AgentServer
new-server\start_distributeserver.bat     # DistributeServer
new-server\start_mapserver.bat           # MapServer
new-server\start_murimnetserver.bat     # MurimNetServer

# 或使用部署包中的脚本
deploy_package\start_servers.bat          # 启动所有服务器
deploy_package\stop_servers.bat           # 停止所有服务器
```

**常见的 Windows 构建问题**：
- **错误：找不到 Boost.Asio**
  ```batch
  # 使用 vcpkg 安装
  vcpkg install boost-asio:x64-windows
  vcpkg integrate install
  ```

- **错误：找不到 PostgreSQL**
  ```batch
  # 安装 PostgreSQL 15+ 或使用 vcpkg
  vcpkg install libpq:x64-windows
  ```

- **错误：CMake 未找到编译器**
  ```batch
  # 指定生成器
  cmake .. -G "Visual Studio 17 2022" -A x64
  ```

**数据库设置**：
```bash
# 初始化PostgreSQL数据库
cd new-server/scripts
psql -U postgres -f init_database_complete.sql

# 检查状态
check_status.bat  # Windows
# 或
psql -U postgres -d mh_game -c "\dt"  # Linux
```

**运行服务器**：
```bash
# AgentServer
./bin/murim_agentserver ../config/agentserver_config.json

# DistributeServer
./bin/murim_distributeserver ../config/distributeserver_config.json

# MapServer
./bin/murim_mapserver ../config/mapserver_config.json

# MurimNetServer
./bin/murim_murimnetserver ../config/murimnetserver_config.json
```

### 客户端开发

**前置要求**：
- Unreal Engine 5.7
- Visual Studio 2019+（Windows）

**开发流程**：
1. 在UE5.7编辑器中打开 `new-client/MurimClient.uproject`
2. 在 `Source/` 目录中修改C++代码
3. 在编辑器中创建蓝图
4. 按Play在编辑器中测试
5. 通过文件 → 打包项目 → Windows打包（用于部署）

**资产导入**：
```bash
cd new-client/Scripts

# 快速批量导入
one_click_import.bat

# 单独资产类型导入
auto_import.bat  # 自动化导入
```

### UE5 客户端开发（Windows 特定）

**环境准备**：
```batch
# 1. 安装 Unreal Engine 5.7
#    从 Epic Games Launcher 安装

# 2. 安装 Visual Studio 2019 或更高版本
#    必需组件：
#    - 使用 C++ 的游戏开发
#    - Windows 10 SDK（或更高）

# 3. 修复项目设置（如果需要）
cd new-client
fix_ue5_project.bat

# 4. 诊断问题
diagnose_ue5.bat
```

**编译选项**：
```batch
# 选项1：使用构建脚本（推荐）
cd new-client
build_now.bat

# 选项2：使用 UBT（Unreal Build Tool）
build_with_ubt.bat

# 选项3：在编辑器中编译
# 打开项目 → 编辑 → 编译（热重载）

# 选项4：清理后重新构建
clean_path_and_build.bat
```

**开发工作流**：
```batch
# 1. 打开项目
双击 new-client\MurimClient.uproject

# 2. 修改 C++ 代码
# 编辑 new-client/Source/ 中的 .h 和 .cpp 文件

# 3. 在编辑器中热重载
# 编辑器会自动检测更改并提示编译

# 4. 编写蓝图
# 内容浏览器 → 创建蓝图 → 选择基类

# 5. 测试
# 按 Play 按钮或在编辑器中运行

# 6. 打包（用于发布）
# 文件 → 打包项目 → Windows → 构建配置
```

### 协议修改

修改网络协议时：

```bash
cd new-server

# 1. 编辑.proto文件
edit proto/messages.proto

# 2. 重新生成C++类
protoc --cpp_out=src/proto --proto_path=proto proto/*.proto

# 3. 复制到客户端
cp src/proto/*.pb.h new-client/Source/Network/Protobuf/
cp src/proto/*.pb.cc new-client/Source/Network/Protobuf/

# 4. 重新构建服务端和客户端
```

## 代码架构详情

### 核心系统 (new-server/src/core/)

**network/** - 基于Boost.Asio的异步网络
- `Connection.hpp` - TCP连接管理
- `Packet.hpp` - 数据包结构：[长度][消息ID][Protobuf数据]
- `Session.hpp` - 会话抽象
- `MessageRouter.hpp` - 分发到处理器

**database/** - PostgreSQL访问层
- `DatabasePool.hpp` - 连接池
- `TransactionHelper.hpp` - 事务管理
- `dao/` - 每个实体的DAO类

**protocol/** - Protocol Buffers
- 所有消息定义在 `new-server/proto/*.proto`
- 生成的C++类在 `src/protocol/`

**spatial/** - 空间索引
- 基于瓦片的空间分区
- 用于战斗、AI、技能的范围查询

**session/** - 会话管理
- 跨服务器的用户会话
- 会话令牌和验证

**security/** - 安全特性
- 密码哈希（SHA-256）
- 数据包加密支持
- 输入验证

### 共享游戏逻辑 (new-server/src/shared/)

**character/** - 角色系统
- `Character.hpp` - 玩家角色数据
- `CharacterManager.hpp` - 生命周期管理

**battle/** - 战斗系统（100%完成）
- `DamageCalculator.hpp` - 17个战斗属性
- `SpecialStateManager.hpp` - 增益/减益
- `AttackManager.hpp` - 战斗执行

**skill/** - 技能系统（98%完成）
- 28种技能对象类型
- 冷却时间管理
- 技能施放逻辑

**ai/** - AI系统（86%完成）
- 状态机：站立、休息、游走、追击、攻击、死亡
- 3秒更新间隔
- 寻路集成

**item/** - 物品系统（97%完成）
- 装备、消耗品、任务物品
- 物品模板和实例
- 背包管理

**quest/** - 任务系统（100%完成）
- 任务进度跟踪
- 任务完成逻辑
- 奖励分发

**social/** - 社交系统（100%完成）
- `GuildManager.hpp` - 帮派创建、管理、战争
- `PartyManager.hpp` - 队伍系统
- `FriendManager.hpp` - 好友列表

**movement/** - 移动系统
- 位置更新
- 移动验证
- 速度计算

**game/** - GameObject基础设施
- `GameObject.hpp` - 基类（17个战斗属性）
- `GameObjectFactory.hpp` - 对象创建
- `ObjectManager.hpp` - 对象生命周期

### 协议参考

**旧版二进制协议**（仅供参考）：
- 文件：`legacy-source-code/[CC]Header/Protocol.h`
- 100+消息类别（MP_CHAR, MP_ITEM, MP_SKILL等）
- 格式：[2B 长度][1B 类别][1B 协议][NB 数据]

**新版Protocol Buffers**：
- 目录：`new-server/proto/`
- 文件：
  - `common.proto` - 共享类型
  - `account.proto` - 认证和账户管理
  - `character.proto` - 角色数据
  - `movement.proto` - 位置/移动
  - `battle.proto` - 战斗消息
  - `skill.proto` - 技能施放
  - `item.proto` - 物品操作
  - `chat.proto` - 聊天消息
  - `quest.proto` - 任务进度
  - `guild.proto` - 帮派操作
  - `party.proto` - 队伍系统
  - `friend.proto` - 好友系统
  - `npc.proto` - NPC 交互
  - `messages.proto` - 消息定义主文件

### 数据库架构

**两个数据库**：

1. **mh_member** - 账户和会话
   - `accounts` - 用户账户
   - `sessions` - 活跃会话

2. **mh_game** - 游戏数据
   - `characters` - 角色记录
   - `characters_item` - 角色物品
   - `characters_skill` - 角色技能
   - `guild_info` - 帮派数据
   - `party_info` - 队伍数据
   - `quest_info` - 任务进度

**架构脚本**：`new-server/scripts/init_database_complete.sql`

## 测试

### 测试结构

**单元测试**（`new-server/tests/unit/`）：
- 单个组件测试
- 示例：`test_bytebuffer.cpp`, `test_database_pool.cpp`

**集成测试**（`new-server/tests/integration/`）：
- 端到端工作流
- 示例：`test_character_flow.cpp`

**运行测试**：
```bash
cd new-server/build
ctest --output-on-failure

# 或运行特定测试
./bin/test_character_manager
```

### 测试数据库
- `mh_game_test` - 游戏数据测试
- `mh_auth_test` - 认证测试
- 由测试框架自动创建

## 资源管理

### 旧版资产格式
- `.4dd` - 打包数据文件
- `.u4d` - 未打包数据
- `.mesh` - 3D模型
- `.anm` - 动画
- `.tga` - 纹理

### 资产迁移流程（详细）

#### 第1步：从旧版格式导出

**旧版资产位置**：
```
legacy-client/
├── Image/2D/          # UI 纹理（.tif, .dds）
├── Image/3D/          # 3D 材质
├── Image/Effect/       # 特效纹理
├── Image/LoadingMap/  # 加载界面图片
├── Sound/             # 音效文件
└── Mesh/              # 3D 模型（旧版格式）
```

**导出工具**：
```batch
# 使用导出工具转换旧资产为 FBX
cd legacy-source-code\[Client]MH\Tools

# 导出单个模型
MAXEXP.exe model.mesh

# 批量导出
convert_all_models.bat
```

**输出位置**：
```
exported_fbx/
├── entry_XXXX.fbx           # 角色和NPC模型
├── monster_entry_XXXX.fbx   # 怪物模型
├── skeleton_XXX.fbx         # 骨架
└── test_single.fbx          # 测试模型
```

#### 第2步：导入到UE5

**方式1：一键批量导入（推荐）**：
```batch
cd new-client\Scripts

# 完全自动导入（包含所有类型）
one_click_import.bat

# 此脚本会：
# 1. 扫描 exported_fbx/ 目录
# 2. 自动分类资产
# 3. 调用 UE5 C# 命令行工具
# 4. 导入到 new-client/Content/
# 5. 生成 .uasset 和 .umap 文件
```

**方式2：分步导入（更可控）**：
```batch
# 步骤1：准备导入
cd new-client\Scripts
start_first_import.bat

# 步骤2：自动导入
start_auto_import.bat

# 步骤3：批量导入
run_batch_import.bat

# 步骤4：验证导入结果
open_import_location.bat
```

**方式3：手动导入（完全控制）**：
```
1. 在 UE5 编辑器中打开项目
2. 内容浏览器 → 导入按钮
3. 选择文件类型过滤器（.fbx, .png, .wav 等）
4. 浏览到 exported_fbx/ 目录
5. 选择文件并配置导入设置：
   - FBX：启用导入材质、启用导入纹理
   - 纹理：压缩设置、MIP 生成
   - 音频：采样率、压缩格式
6. 点击导入
```

**方式4：使用资产导入工具**：
```batch
cd new-client\Scripts

# 打开专用导入工具
asset_import_tool.bat

# 此工具提供：
# - 可视化资产选择
# - 批量导入队列
# - 进度显示
# - 错误日志
```

#### 第3步：资产分类和整理

**UE5 内容目录结构**：
```
new-client/Content/
├── Characters/           # 角色模型和材质
│   ├── Player/          # 玩家角色
│   ├── NPC/            # NPC
│   └── Monsters/       # 怪物
├── Weapons/             # 武器
│   ├── Sword/          # 剑
│   ├── Bow/            # 弓
│   └── Staff/          # 法杖
├── UI/                  # UI 纹理和材质
├── Maps/                # 游戏地图
├── Audio/               # 音效和音乐
│   ├── SFX/            # 音效
│   ├── BGM/            # 背景音乐
│   └── Voice/          # 语音
├── Effects/             # 特效
└── Materials/           # 共享材质库
```

**批量重命名和组织**：
```batch
# 使用脚本组织导入的资产
cd new-client\Scripts
organize_imported_assets.bat
```

#### 第4步：服务器资源配置

**游戏数据资源**（服务器端）：
```
new-server/Resource/
├── items/               # 物品模板数据
│   ├── equipment.json
│   ├── consumables.json
│   └── quest_items.json
├── skills/              # 技能数据
│   ├── skill_trees.json
│   └── skill_effects.json
├── monsters/            # 怪物配置
│   └── monster_templates.json
├── quests/              # 任务数据
│   └── quest_list.json
├── maps/                # 地图配置
│   └── map_data.json
└── config/              # 全局配置
    └── game_config.json
```

**资源加载流程**：
```cpp
// ResourceManager 在服务器启动时自动加载
// 位于 new-server/src/core/resource/ResourceManager.hpp

class ResourceManager {
public:
    // 加载所有游戏数据
    void LoadAllResources();

    // 获取物品模板
    const ItemTemplate* GetItemTemplate(uint32_t item_id);

    // 获取技能数据
    const SkillData* GetSkillData(uint32_t skill_id);

    // 获取怪物模板
    const MonsterTemplate* GetMonsterTemplate(uint32_t monster_id);
};
```

#### 资产导入故障排除

**常见问题**：
```batch
# 问题1：FBX 导入失败
# 解决：检查 FBX 版本（推荐 FBX 2020）
new-client\Scripts\fix_fbx_version.bat

# 问题2：纹理缺失
# 解决：重新导入纹理
new-client\Scripts\reimport_textures.bat

# 问题3：材质未连接
# 解决：在 UE5 编辑器中打开材质并重新保存

# 问题4：资产路径错误
# 解决：使用修复脚本
new-client\fix_ue5_project.bat
```

**验证导入完整性**：
```batch
# 检查导入统计
cd new-client\Scripts
test_single_dds.bat

# 生成导入报告
generate_import_report.bat
```

## 配置文件

### 服务器配置

**agentserver_config.json**：
```json
{
  "server_id": 7000,
  "max_connections": 2000,
  "tick_rate": 100,
  "database": {
    "host": "localhost",
    "port": 5432,
    "database": "mh_member"
  },
  "network": {
    "listen_port": 7000,
    "max_packet_size": 4096
  }
}
```

**mapserver_config.json**：
```json
{
  "server_id": 9001,
  "map_id": 1,
  "max_players": 1000,
  "tick_rate": 100,
  "ai": {
    "process_interval": 3000
  }
}
```

**distributeserver_config.json**：
```json
{
  "server_id": 8000,
  "max_connections": 5000
}
```

### 客户端配置

**DefaultEngine.ini** - UE5引擎设置
**DefaultGame.ini** - 游戏特定设置

## 开发指南

### 添加新功能时

1. **首先检查旧版实现**：
   - 在 `legacy-source-code/` 中查找等效代码
   - 完全理解原始行为
   - 记录任何差异（1:1一致性应该没有差异）

2. **协议更改**：
   - 更新 `.proto` 文件
   - 重新生成C++类
   - 同时更新服务端和客户端

3. **数据库更改**：
   - 修改 `scripts/init_database_complete.sql`
   - 更新相应的DAO类
   - 为现有数据添加迁移脚本

4. **测试**：
   - 为新功能添加单元测试
   - 为工作流添加集成测试
   - 尽可能使用真实客户端测试

5. **文档**：
   - 如果架构更改，更新此CLAUDE.md
   - 为复杂逻辑添加内联注释

### Windows 客户端打包

**开发版本（Development）**：
```
1. 打开项目 → 编辑器插件 → 项目启动器
2. 文件 → 打包项目 → Windows → 构建配置
3. 选择 "Development"
4. 点击"打包项目"
```

**发布版本（Shipping）**：
```
1. 编辑 → 项目设置
2. 搜索 "Packaging"
3. 设置：
   - Build Configuration: Shipping
   - Use Pak File: true
   - Include Symbols: false
4. 文件 → 打包项目 → Windows → 构建配置
5. 点击"打包项目"
```

**快速打包脚本**：
```batch
cd new-client

# 完整重新构建和打包
full_rebuild.bat

# 或使用编辑器命令行
UE5Editor.exe MurimClient.uproject -run=BuildCookRun
```

### 代码风格

**C++17（服务端）**：
- 使用 `std::unique_ptr`, `std::shared_ptr` 进行内存管理
- 类型明显时优先使用 `auto`
- 使用 `namespace` 组织
- 遵循现有命名约定：
  - 类：`PascalCase`
  - 函数：`PascalCase`
  - 成员：`snake_case_`（尾随下划线）
  - 常量：`kPascalCase`

**Unreal Engine 5**：
- 遵循UE5编码标准
- 使用 `UFUNCTION()`, `UCLASS()`, `UPROPERTY()` 宏
- 蓝图可调用函数应为 `BlueprintCallable`
- 使用 `FString` 进行UE5集成

### 常见陷阱

**网络问题**：
- 始终检查连接超时
- 处理数据包碎片
- 验证所有客户端输入（永远不信任客户端）

**数据库问题**：
- 对多表操作使用事务
- 始终检查SQL注入（使用参数化查询）
- 优雅地处理连接失败

**性能问题**：
- MapServer刻度率为100ms - 不要阻塞
- AI每3秒更新一次 - 保持AI简单
- 对范围查询使用空间索引
- 尽可能批量写数据库

**内存管理**：
- 使用智能指针，从不单独使用 `new`/`delete`
- 注意循环引用（`weak_ptr` 打破循环）
- ObjectManager跟踪所有GameObject

## 当前实现状态

### 服务端（C++17）
- **完成**（100%）：账户、角色、战斗、任务、帮派、队伍、物品
- **接近完成**：技能（98%）、AI（86%）、移动（95%）
- **进行中**：网络层集成
- **未开始**：性能优化

### 客户端（UE5.7）
- **完成**：网络层、协议集成
- **进行中**：游戏系统、UI
- **未开始**：完整资产导入、完整游戏玩法

### 工具
- **完成**：代码分析器、协议转换器、数据库转换器

## 故障排除

### 服务器无法启动
- 检查PostgreSQL是否运行：`check_status.bat`
- 验证配置文件存在且JSON有效
- 检查端口可用性（netstat/an）
- 查看 `logs/` 目录中的日志

### 客户端无法连接
- 验证服务器正在运行
- 检查防火墙设置
- 验证客户端配置中的服务器IP/端口
- 检查网络协议版本匹配

### 数据库错误
- 验证数据库存在：`psql -U postgres -l`
- 检查配置中的连接设置
- 验证DAO查询与架构匹配
- 查看PostgreSQL日志

### 构建错误
- 确保使用C++20编译器
- 如需要运行 `cmake --clean-first`
- 检查vcpkg依赖已安装
- 验证找到Boost.Asio和PostgreSQL库

## 重要文件参考

### 服务端入口点
- `new-server/src/agent-server/main/AgentServerMain.cpp`
- `new-server/src/map-server/main/MapServerMain.cpp`
- `new-server/src/distribute-server/main/DistributeServerMain.cpp`

### 关键类
- `new-server/src/shared/game/GameObject.hpp` - 基础实体
- `new-server/src/shared/character/Character.hpp` - 玩家数据
- `new-server/src/shared/battle/DamageCalculator.hpp` - 战斗逻辑
- `new-server/src/core/network/Connection.hpp` - 网络
- `new-server/src/core/database/dao/CharacterDAO.hpp` - 角色持久化

### 协议定义
- `new-server/proto/messages.proto` - 所有消息类型
- `new-server/proto/account.proto` - 认证
- `new-server/proto/character.proto` - 角色数据

### 配置
- `new-server/config/*.json` - 服务器配置
- `new-client/Config/DefaultEngine.ini` - UE5引擎配置

### 旧版参考（请勿修改）
- `legacy-source-code/[CC]Header/Protocol.h` - 原始协议
- `legacy-source-code/[Server]Map/` - 原始游戏逻辑
- `legacy-source-code/[Client]MH/` - 原始客户端

## 项目目标

这是一个**完整的1:1重新实现**，不是重制或灵感项目。原游戏的每个功能都必须存在且功能相同：

- ✅ 所有职业和技能
- ✅ 所有物品和装备
- ✅ 所有任务和故事线
- ✅ 所有帮派和队伍功能
- ✅ 所有战斗机制（PvE和PvP）
- ✅ 所有AI行为
- ✅ 所有社交功能
- ✅ 所有UI元素（在UE5中）
- ✅ 所有资产（模型、纹理、音效、动画）

唯一可接受的更改是技术栈的现代化（C++20、UE5、PostgreSQL），同时保持精确的游戏行为。

有疑问时：**先查看旧版实现，然后1:1实现。**

---

## 📑 完整目录

- [快速参考](#-快速参考) - 最常用的命令
- [项目概述](#项目概述) - 项目介绍和核心要求
- [项目结构](#项目结构) - 目录组织
- [架构概览](#架构概览) - 微服务架构说明
- [构建和开发命令](#构建和开发命令) - 编译和运行指南
  - [服务端开发](#服务端开发)
  - [Windows 平台特定说明](#windows-平台特定说明)
  - [客户端开发](#客户端开发)
  - [UE5 客户端开发（Windows 特定）](#ue5-客户端开发windows-特定)
  - [Windows 客户端打包](#windows-客户端打包)
- [代码架构详情](#代码架构详情) - 核心系统详解
- [协议参考](#协议参考) - 网络协议说明
- [数据库架构](#数据库架构) - 数据库结构
- [测试](#测试) - 测试框架
- [资源管理](#资源管理) - 资产导入流程
  - [资产迁移流程（详细）](#资产迁移流程详细)
- [配置文件](#配置文件) - 服务器和客户端配置
- [开发指南](#开发指南) - 开发规范和最佳实践
  - [添加新功能时](#添加新功能时)
  - [代码风格](#代码风格)
  - [常见陷阱](#常见陷阱)
- [当前实现状态](#当前实现状态) - 项目完成度
- [故障排除](#故障排除) - 常见问题解决
- [重要文件参考](#重要文件参考) - 关键文件列表
- [项目目标](#项目目标) - 开发目标
