#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include "shared/social/Friend.hpp"  // Include full type definitions
#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"

namespace Murim {
namespace Core {
namespace Database {

// Import Game types into Database namespace
using Murim::Game::Friend;
using Murim::Game::FriendRequest;
using Murim::Game::FriendStatus;
using Murim::Game::FriendRequestStatus;

/**
 * @brief 好友数据访问对象 (DAO)
 *
 * 负责好友关系、好友请求的数据库操作
 * 对应 legacy: FriendManager.cpp 数据库部分
 */
class FriendDAO {
public:
    // ========== 好友关系操作 ==========

    /**
     * @brief 添加好友关系（双向）
     * @param character_id 角色ID
     * @param friend_id 好友ID
     * @return 是否成功
     */
    static bool AddFriend(uint64_t character_id, uint64_t friend_id);

    /**
     * @brief 删除好友关系（双向）
     * @param character_id 角色ID
     * @param friend_id 好友ID
     * @return 是否成功
     */
    static bool RemoveFriend(uint64_t character_id, uint64_t friend_id);

    /**
     * @brief 检查是否是好友
     * @param character_id 角色ID
     * @param friend_id 好友ID
     * @return 是否是好友
     */
    static bool IsFriend(uint64_t character_id, uint64_t friend_id);

    /**
     * @brief 获取好友列表（仅ID）
     * @param character_id 角色ID
     * @return 好友ID列表
     */
    static std::vector<uint64_t> GetFriendList(uint64_t character_id);

    /**
     * @brief 获取好友详细信息
     * @param character_id 角色ID
     * @return 好友详细信息列表
     */
    static std::vector<Friend> GetFriendInfo(uint64_t character_id);

    /**
     * @brief 获取单个好友信息
     * @param character_id 角色ID
     * @return 好友信息（不存在返回 nullopt）
     */
    static std::optional<Friend> GetSingleFriendInfo(uint64_t character_id);

    // ========== 好友请求操作 ==========

    /**
     * @brief 创建好友请求
     * @param requester_id 发起者ID
     * @param target_id 目标ID
     * @param message 附带消息
     * @return 请求ID，失败返回0
     */
    static uint64_t CreateFriendRequest(
        uint64_t requester_id,
        uint64_t target_id,
        const std::string& message
    );

    /**
     * @brief 获取待处理的好友请求
     * @param character_id 角色ID
     * @return 好友请求列表
     */
    static std::vector<FriendRequest> GetPendingRequests(uint64_t character_id);

    /**
     * @brief 更新好友请求状态
     * @param request_id 请求ID
     * @param status 新状态
     * @return 是否成功
     */
    static bool UpdateRequestStatus(
        uint64_t request_id,
        FriendRequestStatus status
    );

    /**
     * @brief 删除好友请求
     * @param request_id 请求ID
     * @return 是否成功
     */
    static bool DeleteFriendRequest(uint64_t request_id);

    /**
     * @brief 清理过期的好友请求
     * @param expire_days 过期天数
     * @return 删除的请求数量
     */
    static size_t CleanupExpiredRequests(uint32_t expire_days);

    // ========== 在线状态管理 ==========

    /**
     * @brief 更新角色在线状态
     * @param character_id 角色ID
     * @param status 在线状态
     * @return 是否成功
     */
    static bool UpdateOnlineStatus(
        uint64_t character_id,
        FriendStatus status
    );

    /**
     * @brief 获取角色在线状态
     * @param character_id 角色ID
     * @return 在线状态
     */
    static std::optional<FriendStatus> GetOnlineStatus(uint64_t character_id);

    // ========== 好友统计 ==========

    /**
     * @brief 获取好友数量
     * @param character_id 角色ID
     * @return 好友数量
     */
    static size_t GetFriendCount(uint64_t character_id);

    /**
     * @brief 检查好友列表是否已满
     * @param character_id 角色ID
     * @param max_friends 最大好友数
     * @return 是否已满
     */
    static bool IsFriendListFull(
        uint64_t character_id,
        size_t max_friends
    );
};

} // namespace Database
} // namespace Core
} // namespace Murim
