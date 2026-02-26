#include "Acceptor.hpp"
#include "IOContext.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Network {

Acceptor::Acceptor(IOContext& io_context, const std::string& host, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context.GetIOContext()) {

    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::make_address(host),
        port
    );

    boost::system::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        spdlog::error("Failed to open acceptor: {}", ec.message());
        throw std::runtime_error("Failed to open acceptor");
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint, ec);
    if (ec) {
        spdlog::error("Failed to bind acceptor: {}", ec.message());
        throw std::runtime_error("Failed to bind acceptor");
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        spdlog::error("Failed to listen on acceptor: {}", ec.message());
        throw std::runtime_error("Failed to listen on acceptor");
    }

    spdlog::info("Acceptor listening on {}:{}", host, port);
}

Acceptor::~Acceptor() {
    Stop();
}

void Acceptor::Start(AcceptHandler handler) {
    accept_handler_ = handler;
    DoAccept();
}

void Acceptor::Stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
    if (ec) {
        spdlog::error("Error closing acceptor: {}", ec.message());
    }
}

void Acceptor::DoAccept() {
    auto conn_id = next_conn_id_++;

    spdlog::debug("Accepting new connection, id={}", conn_id);

    acceptor_.async_accept(
        [this, conn_id](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
            if (!ec) {
                spdlog::info("Connection accepted, id={}", conn_id);

                // 使用新构造函数创建 Connection 对象,传入已连接的 socket
                auto connection = std::make_shared<Connection>(io_context_, std::move(socket), conn_id);

                printf("[DEBUG] Acceptor: Calling handler for connection %llu\n", conn_id);
                fflush(stdout);

                if (accept_handler_) {
                    accept_handler_(connection);
                }

                // 继续接受下一个连接
                DoAccept();
            } else {
                spdlog::error("Accept error: {}", ec.message());
            }
        }
    );
}

} // namespace Network
} // namespace Core
} // namespace Murim
