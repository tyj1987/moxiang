#include "../managers/MapServerManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cstdlib>
#include <csignal>
#include <exception>
#include <chrono>
#include <thread>

namespace Murim {
namespace MapServer {

// 全局服务器实例引用
MapServerManager& g_server = MapServerManager::Instance();

// 信号处理
void SignalHandler(int signal) {
    spdlog::info("Received signal: {}", signal);
    g_server.Shutdown();
}

/**
 * @brief MapServer主入口
 *
 * 对应 legacy: main()函数,启动游戏服务器
 */
int main(int argc, char* argv[]) {
    // 设置日志级别
    spdlog::set_level(spdlog::level::info);

    spdlog::info("================================================");
    spdlog::info("       Murim MMORPG MapServer");
    spdlog::info("================================================");
    spdlog::info("Version: 1.0.0");
    spdlog::info("Legacy: Korean MMORPG (Wuxia Theme)");
    spdlog::info("================================================");

    try {
        // 注册信号处理
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // 初始化服务器
        spdlog::info("Step 1: Initialize MapServer...");
        if (!g_server.Initialize()) {
            spdlog::critical("Failed to initialize MapServer!");
            return EXIT_FAILURE;
        }

        // 启动服务器
        spdlog::info("Step 2: Start MapServer...");
        if (!g_server.Start()) {
            spdlog::critical("Failed to start MapServer!");
            return EXIT_FAILURE;
        }

        spdlog::info("Step 3: MapServer running... (Press Ctrl+C to stop)");
        spdlog::info("================================================");

        // 进入主循环
        g_server.MainLoop();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("MapServer shutdown complete. Thank you for playing!");
    spdlog::info("================================================");

    return EXIT_SUCCESS;
}

} // namespace MapServer
} // namespace Murim

// 如果在Windows上,需要真正的main函数
#ifdef _WIN32
int main() {
    return Murim::MapServer::main(0, nullptr);
}
#endif
