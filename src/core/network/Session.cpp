#include "Session.hpp"
#include "IOContext.hpp"
#include "spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Network {

Session::Session(Connection::Ptr connection, uint64_t session_id)
    : connection_(connection),
      session_id_(session_id),
      user_data_(nullptr) {
    UpdateLastActiveTime();
    spdlog::debug("Session created: id={}", session_id_);
}

Session::~Session() {
    spdlog::debug("Session destroyed: id={}", session_id_);
}

void Session::UpdateLastActiveTime() {
    last_active_ = std::chrono::system_clock::now();
}

void Session::Close() {
    if (connection_) {
        connection_->Close();
    }
}

} // namespace Network
} // namespace Core
} // namespace Murim
