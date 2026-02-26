#pragma once

#include <memory>
#include <functional>
#include <array>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/strand.hpp>
#include "ByteBuffer.hpp"

namespace Murim {
namespace Core {
namespace Network {

// Constants
constexpr size_t MAX_BUFFER_SIZE = 8192;

// Forward declarations
class IOContext;

/**
 * @brief TCP connection class
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using Socket = boost::asio::ip::tcp::socket;
    using MessageHandler = std::function<void(const ByteBuffer&)>;

    Connection(IOContext& io_context, uint64_t conn_id);
    Connection(IOContext& io_context, Socket socket, uint64_t conn_id);
    ~Connection();

    Socket& GetSocket();
    uint64_t GetConnectionId() const { return conn_id_; }

    void AsyncConnect(
        const std::string& host,
        uint16_t port,
        std::function<void(const boost::system::error_code&)> handler);

    void AsyncSend(
        std::shared_ptr<ByteBuffer> buffer,
        std::function<void(const boost::system::error_code&, size_t)> handler);

    void AsyncReceive();
    void Close();

    void SetMessageHandler(MessageHandler handler) {
        message_handler_ = handler;
    }

    std::string GetRemoteAddress() const;
    bool IsOpen() const;

private:
    void DoReceive();
    void HandleReceive(const boost::system::error_code& ec, size_t bytes_transferred);
    void HandleError(const boost::system::error_code& ec);

    IOContext& io_context_;
    Socket socket_;
    uint64_t conn_id_;

    // Receive buffer
    std::array<uint8_t, MAX_BUFFER_SIZE> recv_buffer_;

    // Message handler
    MessageHandler message_handler_;

    // Send queue
    std::shared_ptr<ByteBuffer> sending_buffer_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
