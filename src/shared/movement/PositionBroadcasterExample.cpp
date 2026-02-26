/**
 * @file PositionBroadcasterExample.cpp
 * @brief PositionBroadcaster 使用示例
 *
 * 展示如何使用 PositionBroadcaster 进行位置广播
 */

#include "PositionBroadcaster.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace Murim;
using namespace Shared;
using namespace Movement;

/**
 * @brief 示例发送回调函数
 *
 * 实际应用中，这个函数应该通过 ConnectionManager 发送消息给客户端
 */
void ExampleSendCallback(uint64_t target_character_id,
                      const std::vector<uint8_t>& data) {
    std::cout << "[SendCallback] Sending to character " << target_character_id
              << ", data size: " << data.size() << " bytes" << std::endl;

    // 在实际应用中，这里会调用：
    // auto conn = connection_manager->GetConnectionByCharacterId(target_character_id);
    // if (conn) {
    //     conn->Send(data);
    // }
}

/**
 * @brief 示例：模拟1000个玩家移动并广播
 */
void Example1_BasicBroadcasting() {
    std::cout << "\n=== Example 1: Basic Position Broadcasting ===" << std::endl;

    // 1. 创建配置
    PositionBroadcastConfig config;
    config.aoi_radius = 150.0f;         // 150米 AOI
    config.grid_size = 50.0f;            // 50米网格
    config.position_threshold = 0.5f;    // 0.5米阈值
    config.batch_size = 20;               // 每批20个
    config.broadcast_interval = 100;       // 100ms广播间隔

    // 2. 创建广播器
    PositionBroadcaster broadcaster(config, ExampleSendCallback);

    // 3. 模拟1000个玩家
    const int NUM_PLAYERS = 1000;
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        uint64_t character_id = 1000 + i;

        // 随机位置（在1000x1000的区域）
        float x = (rand() % 1000) - 500.0f;
        float y = (rand() % 1000) - 500.0f;
        float z = 0.0f;
        uint16_t direction = rand() % 360;
        uint8_t movement_type = rand() % 3;  // 0=站立, 1=走路, 2=跑步

        broadcaster.UpdatePosition(character_id, x, y, z, direction, movement_type);
    }

    std::cout << "Added " << NUM_PLAYERS << " players" << std::endl;

    // 4. 广播位置更新
    broadcaster.Broadcast();

    // 5. 获取统计信息
    auto stats = broadcaster.GetStatistics();
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  - Total updates: " << stats.total_updates << std::endl;
    std::cout << "  - Broadcast count: " << stats.broadcast_count << std::endl;
    std::cout << "  - Skipped updates: " << stats.skipped_updates << std::endl;
}

/**
 * @brief 示例：模拟玩家连续移动
 */
void Example2_ContinuousMovement() {
    std::cout << "\n=== Example 2: Continuous Movement ===" << std::endl;

    PositionBroadcastConfig config;
    config.aoi_radius = 100.0f;
    config.grid_size = 50.0f;
    config.position_threshold = 1.0f;   // 1米阈值

    PositionBroadcaster broadcaster(config, ExampleSendCallback);

    // 创建一个玩家
    uint64_t character_id = 2001;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    uint16_t direction = 90;
    uint8_t movement_type = 2;  // 跑步

    std::cout << "Simulating player " << character_id << " moving..." << std::endl;

    // 模拟10次移动
    for (int i = 0; i < 10; ++i) {
        // 移动2米
        x += 2.0f;
        y += 1.0f;

        bool added = broadcaster.UpdatePosition(
            character_id, x, y, z, direction, movement_type);

        std::cout << "Step " << (i + 1) << ": Position (" << x << ", " << y << ") - "
                  << (added ? "ADDED to queue" : "SKIPPED (threshold)") << std::endl;

        // 每5步广播一次
        if ((i + 1) % 5 == 0) {
            broadcaster.Broadcast();
            std::cout << "  -> Broadcasted!" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    auto stats = broadcaster.GetStatistics();
    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  - Total updates: " << stats.total_updates << std::endl;
    std::cout << "  - Skipped: " << stats.skipped_updates << std::endl;
}

/**
 * @brief 示例：AOI 查询
 */
void Example3_AOIQuery() {
    std::cout << "\n=== Example 3: AOI Query ===" << std::endl;

    PositionBroadcastConfig config;
    config.aoi_radius = 150.0f;
    config.grid_size = 50.0f;

    PositionBroadcaster broadcaster(config, ExampleSendCallback);

    // 添加一些玩家（形成集群）
    struct PlayerPos {
        uint64_t id;
        float x, y;
    };

    std::vector<PlayerPos> players = {
        {3001, 0.0f, 0.0f},      // 中心玩家
        {3002, 20.0f, 0.0f},    // 附近玩家
        {3003, -20.0f, 10.0f},
        {3004, 0.0f, -30.0f},
        {3005, 200.0f, 200.0f}  // 远处玩家（不在 AOI）
    };

    for (const auto& p : players) {
        broadcaster.UpdatePosition(p.id, p.x, p.y, 0.0f, 0, 1);
        std::cout << "Added player " << p.id << " at (" << p.x << ", " << p.y << ")" << std::endl;
    }

    // 广播（只会发送给 AOI 内的玩家）
    std::cout << "\nBroadcasting..." << std::endl;
    broadcaster.Broadcast();

    auto stats = broadcaster.GetStatistics();
    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  - Total updates: " << stats.total_updates << std::endl;
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  PositionBroadcaster Usage Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        Example1_BasicBroadcasting();
        Example2_ContinuousMovement();
        Example3_AOIQuery();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All examples completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
