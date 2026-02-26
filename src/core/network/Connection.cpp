#include "Connection.hpp"
#include "IOContext.hpp"
#include "../spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Network {

Connection::Connection(IOContext& io_context, uint64_t conn_id)
    : io_context_(io_context),
      socket_(io_context.GetIOContext()),
      conn_id_(conn_id) {
}

Connection::Connection(IOContext& io_context, Socket socket, uint64_t conn_id)
    : io_context_(io_context),
      socket_(std::move(socket)),
      conn_id_(conn_id) {
}

Connection::~Connection() {
    Close();
}

Connection::Socket& Connection::GetSocket() {
    return socket_;
}

void Connection::AsyncConnect(
    const std::string& host,
    uint16_t port,
    std::function<void(const boost::system::error_code&)> handler) {

    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::make_address(host),
        port
    );

    socket_.async_connect(endpoint, handler);
}

void Connection::AsyncSend(
    std::shared_ptr<ByteBuffer> buffer,
    std::function<void(const boost::system::error_code&, size_t)> handler) {

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(buffer->GetData(), buffer->GetSize()),
        [this, self, handler, buffer](const boost::system::error_code& ec, size_t bytes_sent) {
            if (ec) {
                HandleError(ec);
            }
            if (handler) {
                handler(ec, bytes_sent);
            }
        }
    );
}

void Connection::AsyncReceive() {
    DoReceive();
}

void Connection::DoReceive() {
    auto self = shared_from_this();

    socket_.async_read_some(
        boost::asio::buffer(recv_buffer_),
        [this, self](const boost::system::error_code& ec, size_t bytes_transferred) {
            if (!ec) {
                HandleReceive(ec, bytes_transferred);
                DoReceive();
            } else {
                HandleError(ec);
            }
        }
    );
}

void Connection::HandleReceive(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (bytes_transferred > 0) {
        // TODO: Log
    }

    if (message_handler_) {
        auto buffer = std::make_shared<ByteBuffer>(recv_buffer_.data(), bytes_transferred);
        message_handler_(*buffer);
    }
}

void Connection::HandleError(const boost::system::error_code& ec) {
    // TODO: Log error
    Close();
}

void Connection::Close() {
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.close(ec);
    }
}

std::string Connection::GetRemoteAddress() const {
    try {
        if (socket_.is_open()) {
            auto endpoint = socket_.remote_endpoint();
            return endpoint.address().to_string();
        }
    } catch (...) {
        // Ignore
    }
    return "";
}

bool Connection::IsOpen() const {
    return socket_.is_open();
}

} // namespace Network
} // namespace Core
} // namespace Murim
