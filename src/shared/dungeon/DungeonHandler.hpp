#pragma once

#include <memory>
#include "core/network/Connection.hpp"
#include "core/network/ByteBuffer.hpp"
#include "protocol/dungeon.pb.h"
#include "DungeonManager.hpp"

namespace Murim {
namespace Game {

/**
 * @brief 副本系统消息处理器
 *
 * 负责处理副本相关的客户端请求，包括：
 * - 获取副本列表
 * - 获取副本详情
 * - 创建/加入/离开副本房间
 * - 准备就绪
 * - 开始副本
 * - 扫荡副本
 *
 * 消息ID范围：0x1601 - 0x160C
 */
class DungeonHandler {
public:
    // ========== 消息处理器 ==========

    /**
     * @brief 处理获取副本列表请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleGetDungeonListRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理获取副本详情请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleGetDungeonDetailRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理创建副本房间请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleCreateDungeonRoomRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理加入副本房间请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleJoinDungeonRoomRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理离开副本房间请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleLeaveDungeonRoomRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理准备就绪请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleReadyRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理开始副本请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleStartDungeonRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    /**
     * @brief 处理扫荡副本请求
     * @param conn 客户端连接
     * @param buffer 数据包缓冲区
     */
    static void HandleSweepDungeonRequest(
        std::shared_ptr<Core::Network::Connection> conn,
        const Core::Network::ByteBuffer& buffer
    );

    // ========== 辅助方法 ==========

    /**
     * @brief 发送获取副本列表响应
     * @param conn 客户端连接
     * @param dungeons 副本列表
     */
    static void SendGetDungeonListResponse(
        Core::Network::Connection::Ptr conn,
        const std::vector<const DungeonDefinition*>& dungeons
    );

    /**
     * @brief 发送获取副本详情响应
     * @param conn 客户端连接
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param character_id 角色ID
     */
    static void SendGetDungeonDetailResponse(
        Core::Network::Connection::Ptr conn,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        uint64_t character_id
    );

    /**
     * @brief 发送创建副本房间响应
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param leader_id 队长ID
     * @param max_members 最大队员数
     * @param current_members 当前队员数
     */
    static void SendCreateDungeonRoomResponse(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        uint64_t leader_id,
        uint32_t max_members,
        uint32_t current_members
    );

    /**
     * @brief 发送加入副本房间响应
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param leader_id 队长ID
     * @param member_ids 队员ID列表
     */
    static void SendJoinDungeonRoomResponse(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        uint64_t leader_id,
        const std::vector<uint64_t>& member_ids
    );

    /**
     * @brief 发送离开副本房间响应
     * @param conn 客户端连接
     * @param success 是否成功
     * @param room_id 房间ID
     */
    static void SendLeaveDungeonRoomResponse(
        Core::Network::Connection::Ptr conn,
        bool success,
        uint64_t room_id
    );

    /**
     * @brief 发送准备就绪响应
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param ready_count 准备就绪人数
     * @param total_count 总人数
     */
    static void SendReadyResponse(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        uint32_t ready_count,
        uint32_t total_count
    );

    /**
     * @brief 发送开始副本响应
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param success 是否成功
     */
    static void SendStartDungeonResponse(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        bool success
    );

    /**
     * @brief 发送扫荡副本响应
     * @param conn 客户端连接
     * @param item_ids 物品ID列表
     * @param item_counts 物品数量列表
     * @param exp_gain 获得的经验
     * @param gold_gain 获得的金币
     * @param remaining_count 剩余扫荡次数
     */
    static void SendSweepDungeonResponse(
        Core::Network::Connection::Ptr conn,
        const std::vector<uint32_t>& item_ids,
        const std::vector<uint32_t>& item_counts,
        uint32_t exp_gain,
        uint32_t gold_gain,
        uint32_t remaining_count
    );

    // ========== 通知 ==========

    /**
     * @brief 发送副本房间创建通知
     * @param conn 客户端连接
     * @param room 房间信息
     */
    static void SendDungeonRoomCreatedNotification(
        Core::Network::Connection::Ptr conn,
        const DungeonRoom& room
    );

    /**
     * @brief 发送副本房间更新通知
     * @param conn 客户端连接
     * @param room 房间信息
     */
    static void SendDungeonRoomUpdatedNotification(
        Core::Network::Connection::Ptr conn,
        const DungeonRoom& room
    );

    /**
     * @brief 发送队员加入通知
     * @param conn 客户端连接
     * @param member_id 队员ID
     * @param member_name 队员名称
     */
    static void SendDungeonMemberJoinedNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t member_id,
        const std::string& member_name
    );

    /**
     * @brief 发送队员离开通知
     * @param conn 客户端连接
     * @param member_id 队员ID
     * @param member_name 队员名称
     */
    static void SendDungeonMemberLeftNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t member_id,
        const std::string& member_name
    );

    /**
     * @brief 发送副本开始通知
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param dungeon_id 副本ID
     * @param member_ids 队员ID列表
     */
    static void SendDungeonStartedNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        uint32_t dungeon_id,
        const std::vector<uint64_t>& member_ids
    );

    /**
     * @brief 发送副本完成通知
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param dungeon_id 副本ID
     * @param difficulty 难度
     * @param clear_time 通关用时
     * @param first_clear 是否首通
     * @param reward_item_ids 奖励物品ID
     * @param reward_counts 物品数量
     */
    static void SendDungeonCompletedNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        uint32_t dungeon_id,
        murim::DungeonDifficulty difficulty,
        uint32_t clear_time,
        bool first_clear,
        const std::vector<uint32_t>& reward_item_ids,
        const std::vector<uint32_t>& reward_counts
    );

    /**
     * @brief 发送副本失败通知
     * @param conn 客户端连接
     * @param room_id 房间ID
     * @param dungeon_id 副本ID
     * @param member_ids 失败的成员ID列表
     */
    static void SendDungeonFailedNotification(
        Core::Network::Connection::Ptr conn,
        uint64_t room_id,
        uint32_t dungeon_id,
        const std::vector<uint64_t>& member_ids
    );

private:
    // ========== 消息ID定义 ==========

    // 副本系统消息ID范围：0x1601 - 0x160C
    static constexpr uint16_t MSG_GET_DUNGEON_LIST_REQUEST = 0x1601;
    static constexpr uint16_t MSG_GET_DUNGEON_LIST_RESPONSE = 0x1602;
    static constexpr uint16_t MSG_GET_DUNGEON_DETAIL_REQUEST = 0x1603;
    static constexpr uint16_t MSG_GET_DUNGEON_DETAIL_RESPONSE = 0x1604;
    static constexpr uint16_t MSG_CREATE_DUNGEON_ROOM_REQUEST = 0x1605;
    static constexpr uint16_t MSG_CREATE_DUNGEON_ROOM_RESPONSE = 0x1606;
    static constexpr uint16_t MSG_JOIN_DUNGEON_ROOM_REQUEST = 0x1607;
    static constexpr uint16_t MSG_JOIN_DUNGEON_ROOM_RESPONSE = 0x1608;
    static constexpr uint16_t MSG_LEAVE_DUNGEON_ROOM_REQUEST = 0x1609;
    static constexpr uint16_t MSG_LEAVE_DUNGEON_ROOM_RESPONSE = 0x160A;
    static constexpr uint16_t MSG_READY_REQUEST = 0x160B;
    static constexpr uint16_t MSG_READY_RESPONSE = 0x160C;
    static constexpr uint16_t MSG_START_DUNGEON_REQUEST = 0x160D;
    static constexpr uint16_t MSG_START_DUNGEON_RESPONSE = 0x160E;
    static constexpr uint16_t MSG_SWEEP_DUNGEON_REQUEST = 0x160F;
    static constexpr uint16_t MSG_SWEEP_DUNGEON_RESPONSE = 0x1610;

    // 通知消息ID
    static constexpr uint16_t MSG_DUNGEON_ROOM_CREATED_NOTIFICATION = 0x1611;
    static constexpr uint16_t MSG_DUNGEON_ROOM_UPDATED_NOTIFICATION = 0x1612;
    static constexpr uint16_t MSG_DUNGEON_MEMBER_JOINED_NOTIFICATION = 0x1613;
    static constexpr uint16_t MSG_DUNGEON_MEMBER_LEFT_NOTIFICATION = 0x1614;
    static constexpr uint16_t MSG_DUNGEON_STARTED_NOTIFICATION = 0x1615;
    static constexpr uint16_t MSG_DUNGEON_COMPLETED_NOTIFICATION = 0x1616;
    static constexpr uint16_t MSG_DUNGEON_FAILED_NOTIFICATION = 0x1617;
};

} // namespace Game
} // namespace Murim
