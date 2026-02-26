#include "IOContext.hpp"
#include "../spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Network {

IOContext::IOContext(size_t thread_count)
    : thread_count_(thread_count == 0 ? std::thread::hardware_concurrency() : thread_count),
      running_(false) {
}

IOContext::~IOContext() {
    if (running_) {
        Stop();
    }
}

void IOContext::Run() {
    running_ = true;

    for (size_t i = 0; i < thread_count_; ++i) {
        threads_.emplace_back([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                // TODO: Log error
            }
        });
    }
}

void IOContext::Stop() {
    if (!running_) {
        return;
    }

    io_context_.stop();
    running_ = false;

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    threads_.clear();
}

boost::asio::io_context& IOContext::GetIOContext() {
    return io_context_;
}

} // namespace Network
} // namespace Core
} // namespace Murim
