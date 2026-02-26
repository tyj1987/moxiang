#include "../managers/AgentServerManager.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cstdlib>
#include <csignal>
#include <exception>
#include <chrono>
#include <thread>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace AgentServer {

// 信号处理
void SignalHandler(int signal) {
    spdlog::info("Received signal: {}", signal);
    AgentServerManager::Instance().Shutdown();
}

/**
 * @brief AgentServer主入口
 *
 * 对应 legacy: AgentServer主函数,处理玩家认证和连接
 */
int main(int argc, char* argv[]) {
    // 设置日志级别
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("================================================");
    spdlog::info("       Murim MMORPG AgentServer");
    spdlog::info("================================================");
    spdlog::info("Version: 1.0.0");
    spdlog::info("Features: Authentication, Character Selection, Load Balancing");
    spdlog::info("Port: 7000 (configurable)");
    spdlog::info("================================================");

    try {
        // 注册信号处理
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // 初始化服务器
        spdlog::info("Step 1: Initialize AgentServer...");
        if (!AgentServerManager::Instance().Initialize()) {
            spdlog::critical("Failed to initialize AgentServer!");
            return EXIT_FAILURE;
        }

        // 启动服务器
        spdlog::info("Step 2: Start AgentServer...");
        if (!AgentServerManager::Instance().Start()) {
            spdlog::critical("Failed to start AgentServer!");
            return EXIT_FAILURE;
        }

        spdlog::info("Step 3: AgentServer running... (Press Ctrl+C to stop)");
        spdlog::info("================================================");

        // 进入主循环
        AgentServerManager::Instance().MainLoop();

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("AgentServer shutdown complete. Thank you for playing!");
    spdlog::info("================================================");

    return EXIT_SUCCESS;
}

} // namespace AgentServer
} // namespace Murim

// 如果在Windows上,需要真正的main函数
#ifdef _WIN32
int main() {
    return Murim::AgentServer::main(0, nullptr);
}
#endif
