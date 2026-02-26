#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>

namespace Murim {
namespace Game {

/**
 * @brief 好友关系状态
 */
enum class FriendStatus : uint8_t {
    kOffline = 0,        // 离线
    kOnline = 1,          // 在线
    kBusy = 2,            // 忙碌
    kAway = 3,            // 离开
    kHidden = 4           // 隐身
};

/**
 * @brief 好友请求状态
 */
enum class FriendRequestStatus : uint8_t {
    kPending = 0,         // 待处理
    kAccepted = 1,        // 已接受
    kRejected = 2,        // 已拒绝
    kExpired = 3          // 已过期 (7天未处理)
};

/**
 * @brief 好友数据
 */
struct Friend {
    uint64_t character_id;          // 角色ID
    std::string character_name;     // 角色名称
    uint16_t level;                 // 等级
    uint8_t job;                    // 职业
    FriendStatus status;            // 在线状态
    std::chrono::system_clock::time_point last_logout;  // 最后登出时间

    /**
     * @brief 是否在线
     */
    bool IsOnline() const {
        return status == FriendStatus::kOnline ||
               status == FriendStatus::kBusy ||
               status == FriendStatus::kAway;
    }

    /**
     * @brief 获取在线持续时间
     */
    std::chrono::seconds GetOnlineDuration() const {
        if (!IsOnline()) {
            return std::chrono::seconds(0);
        }
        // TODO: 从登录时间计算
        return std::chrono::seconds(0);
    }
};

/**
 * @brief 好友请求
 */
struct FriendRequest {
    uint64_t request_id;            // 请求ID
    uint64_t requester_id;          // 发起者ID
    std::string requester_name;     // 发起者名称
    uint64_t target_id;             // 目标ID
    std::string target_name;        // 目标名称
    FriendRequestStatus status;     // 请求状态
    std::string message;            // 附带消息
    std::chrono::system_clock::time_point request_time;  // 请求时间
    std::chrono::system_clock::time_point expire_time;   // 过期时间 (7天后)

    /**
     * @brief 是否已过期
     */
    bool IsExpired() const {
        auto now = std::chrono::system_clock::now();
        return now >= expire_time;
    }

    /**
     * @brief 是否可以处理
     */
    bool CanProcess() const {
        return status == FriendRequestStatus::kPending && !IsExpired();
    }
};

/**
 * @brief 好友管理器
 *
 * 负责好友添加、删除、在线状态管理
 * 对应 legacy: FriendManager.cpp
 */
class FriendManager {
public:
    /**
     * @brief 获取单例实例
     */
    static FriendManager& Instance();

    /**
     * @brief 初始化好友管理器
     * @return 是否初始化成功
     */
    bool Initialize();

    // ========== 好友查询 ==========

    /**
     * @brief 获取角色好友列表
     * @param character_id 角色ID
     * @return 好友列表
     */
    std::vector<Friend> GetFriendList(uint64_t character_id);

    /**
     * @brief 检查是否是好友
     * @param character_id 角色ID
     * @param target_id 目标ID
     * @return 是否是好友
     */
    bool IsFriend(uint64_t character_id, uint64_t target_id);

    /**
     * @brief 获取在线好友
     * @param character_id 角色ID
     * @return 在线好友列表
     */
    std::vector<Friend> GetOnlineFriends(uint64_t character_id);

    // ========== 好友操作 ==========

    /**
     * @brief 发送好友请求
     * @param requester_id 发起者ID
     * @param target_id 目标ID
     * @param message 附带消息
     * @return 请求ID,失败返回 0
     */
    uint64_t SendFriendRequest(
        uint64_t requester_id,
        uint64_t target_id,
        const std::string& message = ""
    );

    /**
     * @brief 处理好友请求
     * @param character_id 角色ID
     * @param request_id 请求ID
     * @param accept 是否接受
     * @return 是否成功
     */
    bool ProcessFriendRequest(
        uint64_t character_id,
        uint64_t request_id,
        bool accept
    );

    /**
     * @brief 删除好友
     * @param character_id 角色ID
     * @param target_id 目标ID
     * @return 是否成功
     */
    bool RemoveFriend(uint64_t character_id, uint64_t target_id);

    // ========== 好友请求查询 ==========

    /**
     * @brief 获取收到的好友请求
     * @param character_id 角色ID
     * @return 好友请求列表
     */
    std::vector<FriendRequest> GetPendingRequests(uint64_t character_id);

    /**
     * @brief 获取发送的好友请求
     * @param character_id 角色ID
     * @return 好友请求列表
     */
    std::vector<FriendRequest> GetSentRequests(uint64_t character_id);

    /**
     * @brief 获取好友请求
     * @param request_id 请求ID
     * @return 好友请求
     */
    std::optional<FriendRequest> GetFriendRequest(uint64_t request_id);

    // ========== 好友数量 ==========

    /**
     * @brief 获取好友数量
     * @param character_id 角色ID
     * @return 好友数量
     */
    size_t GetFriendCount(uint64_t character_id);

    /**
     * @brief 检查好友数量是否已满
     * @param character_id 角色ID
     * @param max_friends 最大好友数
     * @return 是否已满
     */
    bool IsFriendListFull(uint64_t character_id, size_t max_friends = 100);

    // ========== 在线状态管理 ==========

    /**
     * @brief 更新角色在线状态
     * @param character_id 角色ID
     * @param status 在线状态
     */
    void UpdateOnlineStatus(uint64_t character_id, FriendStatus status);

    /**
     * @brief 角色登录
     * @param character_id 角色ID
     */
    void OnCharacterLogin(uint64_t character_id);

    /**
     * @brief 角色登出
     * @param character_id 角色ID
     */
    void OnCharacterLogout(uint64_t character_id);

    // ========== 好友通知 ==========

    /**
     * @brief 好友上线通知
     * @param character_id 上线的角色ID
     */
    void NotifyFriendsOnLogin(uint64_t character_id);

    /**
     * @brief 好友下线通知
     * @param character_id 下线的角色ID
     */
    void NotifyFriendsOnLogout(uint64_t character_id);

private:
    FriendManager() = default;
    ~FriendManager() = default;
    FriendManager(const FriendManager&) = delete;
    FriendManager& operator=(const FriendManager&) = delete;

    // 好友存储: character_id -> (target_id -> Friend)
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, Friend>> friends_;

    // 好友请求存储
    std::unordered_map<uint64_t, FriendRequest> friend_requests_;

    // 在线角色缓存: character_id -> Friend
    std::unordered_map<uint64_t, Friend> online_characters_;

    // 请求ID生成器
    uint64_t next_request_id_ = 1;

    /**
     * @brief 生成请求ID
     */
    uint64_t GenerateRequestId();

    /**
     * @brief 添加好友关系到内存
     */
    void AddFriendRelation(uint64_t character_id, uint64_t target_id);

    /**
     * @brief 从内存移除好友关系
     */
    void RemoveFriendRelation(uint64_t character_id, uint64_t target_id);

    /**
     * @brief 发送好友通知
     */
    void SendFriendNotification(
        uint64_t character_id,
        const std::string& message
    );
};

} // namespace Game
} // namespace Murim
