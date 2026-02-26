#include "../managers/MurimNetServerManager.hpp"
#include "core/database/DatabasePool.hpp"
#include "core/spdlog_wrapper.hpp"
#include <cstdlib>
#include <csignal>
#include <exception>
#include <chrono>
#include <thread>

namespace Murim {
namespace MurimNetServer {

// 全局服务器实例引用
MurimNetServerManager& g_server = MurimNetServerManager::Instance();

// 信号处理
void SignalHandler(int signal) {
    spdlog::info("Received signal: {}", signal);
    g_server.Shutdown();
}

/**
 * @brief MurimNetServer主入口
 *
 * 对应 legacy: MurimNetServer 启动流程
 */
int Main() {
    try {
        // 初始化日志
        spdlog::set_level(spdlog::level::info);
        spdlog::info("MurimNetServer starting...");

        // 初始化数据库连接池
        Murim::Core::Database::ConnectionPool::Instance().Initialize("postgresql://postgres@localhost:5432/mh_game", 25);

        // 初始化服务器
        if (!g_server.Initialize()) {
            spdlog::error("Failed to initialize MurimNetServer");
            return 1;
        }

        // 注册信号处理
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        // 启动服务器
        if (!g_server.Start()) {
            spdlog::error("Failed to start MurimNetServer");
            return 1;
        }

        spdlog::info("MurimNetServer started successfully");

        // 主循环
        while (g_server.IsRunning()) {
            g_server.Update(100);  // 100ms tick
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 停止服务器
        g_server.Stop();
        spdlog::info("MurimNetServer stopped");

        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
}

} // namespace MurimNetServer
} // namespace Murim

int main() {
    return Murim::MurimNetServer::Main();
}
