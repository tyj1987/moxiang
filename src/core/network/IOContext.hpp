#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <boost/asio/io_context.hpp>

namespace Murim {
namespace Core {
namespace Network {

class IOContext {
public:
    explicit IOContext(size_t thread_count = 0);
    ~IOContext();

    void Run();
    void Stop();

    boost::asio::io_context& GetIOContext();
    size_t GetThreadCount() const { return thread_count_; }

private:
    boost::asio::io_context io_context_;
    std::vector<std::thread> threads_;
    size_t thread_count_;
    std::atomic<bool> running_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
