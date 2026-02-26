#include "../managers/DistributeServerManager.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <cstdlib>
#include <csignal>
#include <exception>
#include <chrono>
#include <thread>

namespace Murim {
namespace DistributeServer {

// 信号处理
void SignalHandler(int signal) {
    spdlog::info("Received signal: {}", signal);
    DistributeServerManager::Instance().Shutdown();
}

/**
 * @brief DistributeServer主入口
 *
 * 对应 legacy: DistributeServer主函数,处理负载均衡和服务器发现
 */
int main(int argc, char* argv[]) {
    // 设置日志级别
    spdlog::set_level(spdlog::level::info);

    spdlog::info("================================================");
    spdlog::info("       Murim MMORPG DistributeServer");
    spdlog::info("================================================");
    spdlog::info("Version: 1.0.0");
    spdlog::info("Features: Load Balancing, Server Discovery, Health Check");
    spdlog::info("Port: 8000 (configurable)");
    spdlog::info("================================================");

    try {
        // 注册信号处理
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // 初始化服务器
        spdlog::info("Step 1: Initialize DistributeServer...");
        if (!DistributeServerManager::Instance().Initialize()) {
            spdlog::critical("Failed to initialize DistributeServer!");
            return EXIT_FAILURE;
        }

        // 启动服务器
        spdlog::info("Step 2: Start DistributeServer...");
        if (!DistributeServerManager::Instance().Start()) {
            spdlog::critical("Failed to start DistributeServer!");
            return EXIT_FAILURE;
        }

        spdlog::info("Step 3: DistributeServer running... (Press Ctrl+C to stop)");
        spdlog::info("================================================");

        // 进入主循环
        DistributeServerManager::Instance().MainLoop();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("DistributeServer shutdown complete. Thank you for playing!");
    spdlog::info("================================================");

    return EXIT_SUCCESS;
}

} // namespace DistributeServer
} // namespace Murim

// 如果在Windows上,需要真正的main函数
#ifdef _WIN32
int main() {
    return Murim::DistributeServer::main(0, nullptr);
}
#endif
