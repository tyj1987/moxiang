#pragma once

#include <memory>
#include <functional>
#include <boost/asio/ip/tcp.hpp>
#include "Connection.hpp"

namespace Murim {
namespace Core {
namespace Network {

// 前向声明
class IOContext;

/**
 * @brief 连接接受器
 *
 * 监听端口并接受新连接
 */
class Acceptor : public std::enable_shared_from_this<Acceptor> {
public:
    using Ptr = std::shared_ptr<Acceptor>;
    using AcceptHandler = std::function<void(Connection::Ptr)>;

    /**
     * @brief 构造函数
     * @param io_context I/O 上下文
     * @param host 监听地址
     * @param port 监听端口
     */
    Acceptor(IOContext& io_context, const std::string& host, uint16_t port);

    /**
     * @brief 析构函数
     */
    ~Acceptor();

    /**
     * @brief 启动接受连接
     * @param handler 接受连接后的回调函数
     */
    void Start(AcceptHandler handler);

    /**
     * @brief 停止接受连接
     */
    void Stop();

private:
    void DoAccept();

    IOContext& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    AcceptHandler accept_handler_;
    std::atomic<uint64_t> next_conn_id_{1};
};

} // namespace Network
} // namespace Core
} // namespace Murim
