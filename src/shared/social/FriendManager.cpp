#include "Friend.hpp"
#include "core/network/NotificationService.hpp"
#include "core/database/FriendDAO.hpp"
#include "core/database/CharacterDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include <algorithm>
#include <utility>

// using Database = Murim::Core::Database;  // Removed - causes parsing errors
using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

// ========== FriendManager ==========

FriendManager& FriendManager::Instance() {
    static FriendManager instance;
    return instance;
}

bool FriendManager::Initialize() {
    spdlog::info("FriendManager initialized");
    return true;
}

// ========== 好友查询 ==========

std::vector<Friend> FriendManager::GetFriendList(uint64_t character_id) {
    std::vector<Friend> friends;

    auto it = friends_.find(character_id);
    if (it != friends_.end()) {
        for (const auto& [target_id, friend_data] : it->second) {
            friends.push_back(friend_data);
        }
    }

    // 按在线状态排�?(在线在前)
    std::sort(friends.begin(), friends.end(),
        [](const Friend& a, const Friend& b) {
            return a.IsOnline() && !b.IsOnline();
        });

    spdlog::debug("Character {} has {} friends", character_id, friends.size());

    return friends;
}

bool FriendManager::IsFriend(uint64_t character_id, uint64_t target_id) {
    auto it = friends_.find(character_id);
    if (it == friends_.end()) {
        return false;
    }

    return it->second.find(target_id) != it->second.end();
}

std::vector<Friend> FriendManager::GetOnlineFriends(uint64_t character_id) {
    std::vector<Friend> online_friends;

    auto all_friends = GetFriendList(character_id);

    for (const auto& friend_data : all_friends) {
        if (friend_data.IsOnline()) {
            online_friends.push_back(friend_data);
        }
    }

    spdlog::debug("Character {} has {} online friends", character_id, online_friends.size());

    return online_friends;
}

// ========== 好友操作 ==========

uint64_t FriendManager::SendFriendRequest(
    uint64_t requester_id,
    uint64_t target_id,
    const std::string& message
) {
    // 检查是否已经是好友
    if (IsFriend(requester_id, target_id)) {
        spdlog::warn("Already friends: {} and {}", requester_id, target_id);
        return 0;
    }

    // 检查是否有待处理的请求 (从数据库查询)
    auto pending_requests = Murim::Core::Database::FriendDAO::GetPendingRequests(target_id);
    for (const auto& req : pending_requests) {
        if (req.requester_id == requester_id) {
            spdlog::warn("Friend request already exists: {} -> {}", requester_id, target_id);
            return 0;
        }
    }

    // 创建好友请求
    FriendRequest request;
    request.request_id = GenerateRequestId();
    request.requester_id = requester_id;
    request.target_id = target_id;
    request.status = FriendRequestStatus::kPending;
    request.message = message;
    request.request_time = std::chrono::system_clock::now();
    request.expire_time = request.request_time + std::chrono::hours(24 * 7);  // 7天过�?
    friend_requests_[request.request_id] = request;

    // 保存到数据库
    try {
        auto request_time_t = std::chrono::system_clock::to_time_t(request.request_time);
        auto expire_time_t = std::chrono::system_clock::to_time_t(request.expire_time);

        Murim::Core::Database::FriendDAO::CreateFriendRequest(
            requester_id,
            target_id,
            message
        );
    } catch (const std::exception& e) {
        spdlog::error("Failed to save friend request to database: {}", e.what());
        // 继续执行,内存中已有记录
    }

    // 通知目标玩家
    SendFriendNotification(target_id, "You have received a friend request");

    spdlog::info("Friend request sent: {} -> {}, request_id={}",
                 requester_id, target_id, request.request_id);

    return request.request_id;
}

bool FriendManager::ProcessFriendRequest(
    uint64_t character_id,
    uint64_t request_id,
    bool accept
) {
    auto it = friend_requests_.find(request_id);
    if (it == friend_requests_.end()) {
        spdlog::warn("Friend request not found: {}", request_id);
        return false;
    }

    auto& request = it->second;

    // 验证请求是属于该玩家的
    if (request.target_id != character_id) {
        spdlog::warn("Friend request {} does not belong to character {}",
                     request_id, character_id);
        return false;
    }

    // 检查是否可以处理
    if (!request.CanProcess()) {
        spdlog::warn("Friend request {} cannot be processed", request_id);
        return false;
    }

    if (accept) {
        // 标记请求为已接受
        request.status = FriendRequestStatus::kAccepted;

        spdlog::info("Friend request accepted: {} <-> {}",
                     request.requester_id, request.target_id);
    } else {
        request.status = FriendRequestStatus::kRejected;

        spdlog::info("Friend request rejected: {} -> {}",
                     request.requester_id, request.target_id);
    }

    // 更新数据库
    try {
        // 如果接受,添加双向好友关系
        if (accept) {
            Murim::Core::Database::FriendDAO::AddFriend(request.requester_id, request.target_id);
            Murim::Core::Database::FriendDAO::AddFriend(request.target_id, request.requester_id);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to update database: {}", e.what());
    }

    // 通知发起者
    if (accept) {
        SendFriendNotification(request.requester_id,
                              "Your friend request was accepted");
    } else {
        SendFriendNotification(request.requester_id,
                              "Your friend request was rejected");
    }

    // 清理已处理的请求
    friend_requests_.erase(it);

    return true;
}

bool FriendManager::RemoveFriend(uint64_t character_id, uint64_t target_id) {
    // 检查是否是好友
    if (!IsFriend(character_id, target_id)) {
        spdlog::warn("Not friends: {} and {}", character_id, target_id);
        return false;
    }

    // 更新数据库
    try {
        Murim::Core::Database::FriendDAO::RemoveFriend(character_id, target_id);
        Murim::Core::Database::FriendDAO::RemoveFriend(target_id, character_id);
    } catch (const std::exception& e) {
        spdlog::error("Failed to remove friend from database: {}", e.what());
    }

    spdlog::info("Friend removed: {} <-> {}", character_id, target_id);

    return true;
}

// ========== 好友请求查询 ==========

std::vector<FriendRequest> FriendManager::GetPendingRequests(uint64_t character_id) {
    std::vector<FriendRequest> requests;

    for (const auto& [request_id, request] : friend_requests_) {
        if (request.target_id == character_id &&
            request.CanProcess()) {
            requests.push_back(request);
        }
    }

    spdlog::debug("Character {} has {} pending friend requests",
                 character_id, requests.size());

    return requests;
}

std::vector<FriendRequest> FriendManager::GetSentRequests(uint64_t character_id) {
    std::vector<FriendRequest> requests;

    for (const auto& [request_id, request] : friend_requests_) {
        if (request.requester_id == character_id &&
            request.CanProcess()) {
            requests.push_back(request);
        }
    }

    return requests;
}

std::optional<FriendRequest> FriendManager::GetFriendRequest(uint64_t request_id) {
    auto it = friend_requests_.find(request_id);
    if (it != friend_requests_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ========== 好友数量 ==========

size_t FriendManager::GetFriendCount(uint64_t character_id) {
    auto it = friends_.find(character_id);
    if (it != friends_.end()) {
        return it->second.size();
    }
    return 0;
}

bool FriendManager::IsFriendListFull(uint64_t character_id, size_t max_friends) {
    size_t count = GetFriendCount(character_id);
    return count >= max_friends;
}

// ========== 在线状态管�?==========

void FriendManager::UpdateOnlineStatus(uint64_t character_id, FriendStatus status) {
    // 更新在线角色缓存
    if (status == FriendStatus::kOnline) {
        Friend friend_info = {};  // 使用值初始化
        friend_info.character_id = character_id;
        friend_info.status = status;

        // 从数据库加载角色信息
        try {
            auto db_friends = Murim::Core::Database::FriendDAO::GetFriendInfo(character_id);
            if (!db_friends.empty()) {
                friend_info.character_name = db_friends[0].character_name;
                friend_info.level = db_friends[0].level;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to load character info from database: {}", e.what());
            // 使用默认值
            friend_info.character_name = "Unknown";
            friend_info.level = 1;
        }

        online_characters_[character_id] = friend_info;
    } else {
        online_characters_.erase(character_id);
    }

    // 更新好友列表中的在线状态
    auto it = friends_.find(character_id);
    if (it != friends_.end()) {
        for (auto& pair : it->second) {
            pair.second.status = status;
        }
    }

    spdlog::debug("Character {} status updated: {}",
                 character_id, static_cast<int>(status));
}

void FriendManager::OnCharacterLogin(uint64_t character_id) {
    UpdateOnlineStatus(character_id, FriendStatus::kOnline);
    NotifyFriendsOnLogin(character_id);
}

void FriendManager::OnCharacterLogout(uint64_t character_id) {
    UpdateOnlineStatus(character_id, FriendStatus::kOffline);
    NotifyFriendsOnLogout(character_id);
}

// ========== 好友通知 ==========

void FriendManager::NotifyFriendsOnLogin(uint64_t character_id) {
    auto friends = GetFriendList(character_id);

    // 获取角色名称
    auto character_opt = Murim::Core::Database::CharacterDAO::Load(character_id);
    if (!character_opt.has_value()) {
        spdlog::error("Character {} not found for friend login notification", character_id);
        return;
    }
    std::string character_name = character_opt->name;

    for (const auto& friend_data : friends) {
        // 通知好友: character_id 上线�?- 对应 legacy 好友上线通知
        Core::Network::NotificationService::Instance().NotifyFriendOnline(
            friend_data.character_id, character_id, character_name);
    }

    spdlog::info("Notified {} friends of character {} login",
                 friends.size(), character_id);
}

void FriendManager::NotifyFriendsOnLogout(uint64_t character_id) {
    auto friends = GetFriendList(character_id);

    for (const auto& friend_data : friends) {
        if (friend_data.IsOnline()) {
            // 通知在线好友: character_id 下线�?- 对应 legacy 好友下线通知
            Core::Network::NotificationService::Instance().NotifyFriendOffline(
                friend_data.character_id, character_id);
        }
    }

    spdlog::info("Notified online friends of character {} logout",
                 friends.size(), character_id);
}

// ========== 辅助方法 ==========

void FriendManager::AddFriendRelation(uint64_t character_id, uint64_t target_id) {
    Friend friend_info;
    friend_info.character_id = target_id;

    // 获取好友信息 (从数据库或在线缓存)
    auto online_it = online_characters_.find(target_id);
    if (online_it != online_characters_.end()) {
        // 角色在线,使用缓存信息
        friend_info = online_it->second;
        friend_info.status = FriendStatus::kOnline;
    } else {
        // 角色离线,从数据库加载
        try {
            auto db_friend = Murim::Core::Database::FriendDAO::GetSingleFriendInfo(target_id);
            if (db_friend.has_value()) {
                friend_info.character_name = db_friend->character_name;
                friend_info.level = db_friend->level;
                friend_info.status = FriendStatus::kOffline;
            } else {
                // 数据库中也没有,使用默认值
                friend_info.character_name = "Unknown";
                friend_info.level = 1;
                friend_info.status = FriendStatus::kOffline;
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to load friend info from database: {}", e.what());
            friend_info.character_name = "Unknown";
            friend_info.level = 1;
            friend_info.status = FriendStatus::kOffline;
        }
    }

    friends_[character_id][target_id] = friend_info;
}

void FriendManager::RemoveFriendRelation(uint64_t character_id, uint64_t target_id) {
    auto it = friends_.find(character_id);
    if (it != friends_.end()) {
        it->second.erase(target_id);

        if (it->second.empty()) {
            friends_.erase(it);
        }
    }
}

uint64_t FriendManager::GenerateRequestId() {
    return next_request_id_++;
}

void FriendManager::SendFriendNotification(
    uint64_t character_id,
    const std::string& message
) {
    // 发送好友通知到客户端 - 对应 legacy 好友消息
    Core::Network::NotificationService::Instance().SendSystemMessage(character_id, message);
    spdlog::info("Friend notification to {}: {}", character_id, message);
}

} // namespace Game
} // namespace Murim
