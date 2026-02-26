#include "DungeonHandler.hpp"
#include "core/network/ByteBuffer.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== 消息处理器 ==========

void DungeonHandler::HandleGetDungeonListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::debug("[DungeonHandler] Handling GetDungeonListRequest");

    murim::GetDungeonListRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse GetDungeonListRequest");
        return;
    }

    // 获取所有副本定义
    auto dungeons = DungeonManager::Instance().GetAllDungeonDefinitions();

    // 发送响应
    SendGetDungeonListResponse(conn, dungeons);
}

void DungeonHandler::HandleGetDungeonDetailRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::GetDungeonDetailRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse GetDungeonDetailRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::debug("[DungeonHandler] Handling GetDungeonDetailRequest for dungeon {} by character {}",
        request.dungeon_id(), character_id);

    // 发送响应
    SendGetDungeonDetailResponse(conn, request.dungeon_id(),
        murim::DungeonDifficulty::NORMAL, character_id);
}

void DungeonHandler::HandleCreateDungeonRoomRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::CreateDungeonRoomRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse CreateDungeonRoomRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::info("[DungeonHandler] Character {} creating dungeon room for dungeon {}",
        character_id, request.dungeon_id());

    // 检查副本定义
    const DungeonDefinition* definition =
        DungeonManager::Instance().GetDungeonDefinition(request.dungeon_id());
    if (!definition) {
        spdlog::warn("[DungeonHandler] Dungeon definition {} not found", request.dungeon_id());
        // TODO: 发送错误响应
        return;
    }

    // 检查等级要求
    // TODO: 获取角色等级并检查

    // 创建房间
    uint64_t room_id = DungeonManager::Instance().CreateDungeonRoom(
        character_id,
        request.dungeon_id(),
        request.difficulty()
    );

    if (room_id == 0) {
        spdlog::error("[DungeonHandler] Failed to create dungeon room");
        // TODO: 发送错误响应
        return;
    }

    // 发送响应
    SendCreateDungeonRoomResponse(
        conn,
        room_id,
        request.dungeon_id(),
        request.difficulty(),
        character_id,
        definition->max_players,
        1
    );

    // 发送房间创建通知
    const DungeonRoom* room = DungeonManager::Instance().GetDungeonRoom(room_id);
    if (room) {
        SendDungeonRoomCreatedNotification(conn, *room);
    }
}

void DungeonHandler::HandleJoinDungeonRoomRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::JoinDungeonRoomRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse JoinDungeonRoomRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::info("[DungeonHandler] Character {} joining dungeon room {}",
        character_id, request.room_id());

    // 加入房间
    bool success = DungeonManager::Instance().JoinDungeonRoom(
        request.room_id(),
        character_id
    );

    if (!success) {
        spdlog::warn("[DungeonHandler] Character {} failed to join room {}",
            character_id, request.room_id());
        // TODO: 发送错误响应
        return;
    }

    // 获取房间信息
    const DungeonRoom* room = DungeonManager::Instance().GetDungeonRoom(request.room_id());
    if (!room) {
        spdlog::error("[DungeonHandler] Room {} not found after join", request.room_id());
        return;
    }

    // 发送响应
    SendJoinDungeonRoomResponse(
        conn,
        room->room_id,
        room->dungeon_id,
        room->difficulty,
        room->team.leader_id,
        room->team.member_ids
    );

    // 发送队员加入通知
    SendDungeonMemberJoinedNotification(conn, character_id, "Player"); // TODO: 获取角色名称

    // 发送房间更新通知
    SendDungeonRoomUpdatedNotification(conn, *room);
}

void DungeonHandler::HandleLeaveDungeonRoomRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::LeaveDungeonRoomRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse LeaveDungeonRoomRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::info("[DungeonHandler] Character {} leaving dungeon room {}",
        character_id, request.room_id());

    // 离开房间
    bool success = DungeonManager::Instance().LeaveDungeonRoom(
        request.room_id(),
        character_id
    );

    // 发送响应
    SendLeaveDungeonRoomResponse(conn, success, request.room_id());

    if (success) {
        // 发送队员离开通知
        SendDungeonMemberLeftNotification(conn, character_id, "Player"); // TODO: 获取角色名称
    }
}

void DungeonHandler::HandleReadyRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::ReadyRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse ReadyRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::debug("[DungeonHandler] Character {} set ready {} in room {}",
        character_id, request.ready(), request.room_id());

    // 设置准备状态
    bool success = DungeonManager::Instance().SetReadyStatus(
        request.room_id(),
        character_id,
        request.ready()
    );

    if (!success) {
        spdlog::warn("[DungeonHandler] Failed to set ready status for character {} in room {}",
            character_id, request.room_id());
        return;
    }

    // 获取房间信息
    const DungeonRoom* room = DungeonManager::Instance().GetDungeonRoom(request.room_id());
    if (!room) {
        spdlog::error("[DungeonHandler] Room {} not found", request.room_id());
        return;
    }

    // 发送响应
    SendReadyResponse(
        conn,
        request.room_id(),
        room->team.ready_count,
        static_cast<uint32_t>(room->team.member_ids.size())
    );

    // 发送房间更新通知
    SendDungeonRoomUpdatedNotification(conn, *room);
}

void DungeonHandler::HandleStartDungeonRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::StartDungeonRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse StartDungeonRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::info("[DungeonHandler] Character {} requesting to start dungeon room {}",
        character_id, request.room_id());

    // 检查是否是队长
    const DungeonRoom* room = DungeonManager::Instance().GetDungeonRoom(request.room_id());
    if (!room) {
        spdlog::error("[DungeonHandler] Room {} not found", request.room_id());
        SendStartDungeonResponse(conn, request.room_id(), false);
        return;
    }

    if (room->team.leader_id != character_id) {
        spdlog::warn("[DungeonHandler] Character {} is not leader of room {}",
            character_id, request.room_id());
        SendStartDungeonResponse(conn, request.room_id(), false);
        return;
    }

    // 开始副本
    bool success = DungeonManager::Instance().StartDungeon(request.room_id());

    // 发送响应
    SendStartDungeonResponse(conn, request.room_id(), success);

    if (success && room) {
        // 发送副本开始通知
        SendDungeonStartedNotification(
            conn,
            room->room_id,
            room->dungeon_id,
            room->team.member_ids
        );
    }
}

void DungeonHandler::HandleSweepDungeonRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    murim::SweepDungeonRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[DungeonHandler] Failed to parse SweepDungeonRequest");
        return;
    }

    // TODO: 从连接中获取 character_id
    uint64_t character_id = 0;

    spdlog::info("[DungeonHandler] Character {} sweeping dungeon {} {} times",
        character_id, request.dungeon_id(), request.count());

    // 扫荡副本
    std::vector<uint32_t> items;
    std::vector<uint32_t> item_counts;
    uint32_t exp_gain = 0;
    uint32_t gold_gain = 0;

    bool success = DungeonManager::Instance().SweepDungeon(
        character_id,
        request.dungeon_id(),
        request.difficulty(),
        request.count(),
        items,
        item_counts,
        exp_gain,
        gold_gain
    );

    if (!success) {
        spdlog::warn("[DungeonHandler] Failed to sweep dungeon {} for character {}",
            request.dungeon_id(), character_id);
        // TODO: 发送错误响应
        return;
    }

    // 获取剩余扫荡次数
    uint32_t remaining_count = DungeonManager::Instance().GetRemainingSweepCount(
        character_id,
        request.dungeon_id(),
        request.difficulty()
    );

    // 发送响应
    SendSweepDungeonResponse(
        conn,
        items,
        item_counts,
        exp_gain,
        gold_gain,
        remaining_count
    );
}

// ========== 辅助方法 ==========

void DungeonHandler::SendGetDungeonListResponse(
    Core::Network::Connection::Ptr conn,
    const std::vector<const DungeonDefinition*>& dungeons) {

    murim::GetDungeonListResponse response;
    response.mutable_response()->set_success(true);
    response.mutable_response()->set_message_code(0);

    // 填充副本列表
    for (const DungeonDefinition* def : dungeons) {
        murim::DungeonDefinition* dungeon_def = response.add_dungeons();
        dungeon_def->set_dungeon_id(def->dungeon_id);
        dungeon_def->set_name(def->name);
        dungeon_def->set_description(def->description);
        dungeon_def->set_type(def->type);
        dungeon_def->set_min_level(def->min_level);
        dungeon_def->set_max_level(def->max_level);
        dungeon_def->set_min_players(def->min_players);
        dungeon_def->set_max_players(def->max_players);
        dungeon_def->set_recommend_level(def->recommend_level);
        dungeon_def->set_recommend_item_level(def->recommend_item_level);

        // 奖励
        for (uint32_t item_id : def->normal_rewards) {
            dungeon_def->add_normal_rewards(item_id);
        }
        for (uint32_t item_id : def->first_clear_rewards) {
            dungeon_def->add_first_clear_rewards(item_id);
        }
        dungeon_def->set_exp_reward(def->exp_reward);
        dungeon_def->set_gold_reward(def->gold_reward);

        // 扫荡
        dungeon_def->set_can_sweep(def->can_sweep);
        dungeon_def->set_sweep_unlock_clear_count(def->sweep_unlock_clear_count);

        // 刷新时间
        dungeon_def->set_daily_reset_time(def->daily_reset_time);
        dungeon_def->set_weekly_reset_time(def->weekly_reset_time);
    }

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_GET_DUNGEON_LIST_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent GetDungeonListResponse with {} dungeons", dungeons.size());
}

void DungeonHandler::SendGetDungeonDetailResponse(
    Core::Network::Connection::Ptr conn,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    uint64_t character_id) {

    murim::GetDungeonDetailResponse response;
    response.mutable_response()->set_success(true);
    response.mutable_response()->set_message_code(0);

    // 获取副本定义
    const DungeonDefinition* definition = DungeonManager::Instance().GetDungeonDefinition(dungeon_id);
    if (!definition) {
        response.mutable_response()->set_success(false);
        response.mutable_response()->set_message_code(1); // 副本不存在

        std::vector<uint8_t> buffer(response.ByteSizeLong());
        response.SerializeToArray(buffer.data(), buffer.size());

        Core::Network::Packet packet(MSG_GET_DUNGEON_DETAIL_RESPONSE, buffer);
        conn->Send(packet);
        return;
    }

    // 填充副本定义
    murim::DungeonDefinition* dungeon_def = response.mutable_dungeon();
    dungeon_def->set_dungeon_id(definition->dungeon_id);
    dungeon_def->set_name(definition->name);
    dungeon_def->set_description(definition->description);
    dungeon_def->set_type(definition->type);
    dungeon_def->set_min_level(definition->min_level);
    dungeon_def->set_max_level(definition->max_level);
    dungeon_def->set_min_players(definition->min_players);
    dungeon_def->set_max_players(definition->max_players);
    dungeon_def->set_recommend_level(definition->recommend_level);
    dungeon_def->set_recommend_item_level(definition->recommend_item_level);

    for (uint32_t item_id : definition->normal_rewards) {
        dungeon_def->add_normal_rewards(item_id);
    }
    for (uint32_t item_id : definition->first_clear_rewards) {
        dungeon_def->add_first_clear_rewards(item_id);
    }
    dungeon_def->set_exp_reward(definition->exp_reward);
    dungeon_def->set_gold_reward(definition->gold_reward);
    dungeon_def->set_can_sweep(definition->can_sweep);
    dungeon_def->set_sweep_unlock_clear_count(definition->sweep_unlock_clear_count);
    dungeon_def->set_daily_reset_time(definition->daily_reset_time);
    dungeon_def->set_weekly_reset_time(definition->weekly_reset_time);

    // 获取副本进度
    const DungeonProgress* progress =
        DungeonManager::Instance().GetDungeonProgress(character_id, dungeon_id, difficulty);

    if (progress) {
        murim::DungeonProgress* progress_msg = response.mutable_progress();
        progress_msg->set_dungeon_id(progress->dungeon_id);
        progress_msg->set_difficulty(progress->difficulty);
        progress_msg->set_state(progress->state);
        progress_msg->set_clear_time(progress->clear_time);
        progress_msg->set_progress(progress->progress);
        progress_msg->set_best_clear_time(progress->best_clear_time);
        progress_msg->set_clear_count(progress->clear_count);
        progress_msg->set_today_clear_count(progress->today_clear_count);
        progress_msg->set_today_first_clear_difficulty(progress->today_first_clear_difficulty);
        progress_msg->set_weekly_clear_count(progress->weekly_clear_count);
    }

    // 扫荡信息
    response.set_can_sweep(DungeonManager::Instance().CanSweep(character_id, dungeon_id, difficulty) ? 1 : 0);
    response.set_sweep_count(DungeonManager::Instance().GetRemainingSweepCount(character_id, dungeon_id, difficulty));

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_GET_DUNGEON_DETAIL_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent GetDungeonDetailResponse for dungeon {}", dungeon_id);
}

void DungeonHandler::SendCreateDungeonRoomResponse(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    uint64_t leader_id,
    uint32_t max_members,
    uint32_t current_members) {

    murim::CreateDungeonRoomResponse response;
    response.mutable_response()->set_success(true);
    response.mutable_response()->set_message_code(0);
    response.set_room_id(room_id);
    response.set_dungeon_id(dungeon_id);
    response.set_difficulty(difficulty);
    response.set_leader_id(leader_id);
    response.set_max_members(max_members);
    response.set_current_members(current_members);

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_CREATE_DUNGEON_ROOM_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent CreateDungeonRoomResponse for room {}", room_id);
}

void DungeonHandler::SendJoinDungeonRoomResponse(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    uint64_t leader_id,
    const std::vector<uint64_t>& member_ids) {

    murim::JoinDungeonRoomResponse response;
    response.mutable_response()->set_success(true);
    response.mutable_response()->set_message_code(0);
    response.set_room_id(room_id);
    response.set_dungeon_id(dungeon_id);
    response.set_difficulty(difficulty);
    response.set_leader_id(leader_id);

    for (uint64_t member_id : member_ids) {
        response.add_member_ids(member_id);
    }

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_JOIN_DUNGEON_ROOM_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent JoinDungeonRoomResponse for room {}", room_id);
}

void DungeonHandler::SendLeaveDungeonRoomResponse(
    Core::Network::Connection::Ptr conn,
    bool success,
    uint64_t room_id) {

    murim::LeaveDungeonRoomResponse response;
    response.mutable_response()->set_success(success);
    response.mutable_response()->set_message_code(success ? 0 : 1);
    response.set_success(success);
    response.set_room_id(room_id);

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_LEAVE_DUNGEON_ROOM_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent LeaveDungeonRoomResponse for room {}, success: {}", room_id, success);
}

void DungeonHandler::SendReadyResponse(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    uint32_t ready_count,
    uint32_t total_count) {

    murim::ReadyResponse response;
    response.mutable_response()->set_success(true);
    response.mutable_response()->set_message_code(0);
    response.set_room_id(room_id);
    response.set_ready_count(ready_count);
    response.set_total_count(total_count);

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_READY_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent ReadyResponse for room {}: {}/{} ready",
        room_id, ready_count, total_count);
}

void DungeonHandler::SendStartDungeonResponse(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    bool success) {

    murim::StartDungeonResponse response;
    response.mutable_response()->set_success(success);
    response.mutable_response()->set_message_code(success ? 0 : 1);
    response.set_room_id(room_id);
    response.set_success(success);

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_START_DUNGEON_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::info("[DungeonHandler] Sent StartDungeonResponse for room {}, success: {}", room_id, success);
}

void DungeonHandler::SendSweepDungeonResponse(
    Core::Network::Connection::Ptr conn,
    const std::vector<uint32_t>& item_ids,
    const std::vector<uint32_t>& item_counts,
    uint32_t exp_gain,
    uint32_t gold_gain,
    uint32_t remaining_count) {

    murim::SweepDungeonResponse response;
    response.mutable_response()->set_success(true);
    response.mutable_response()->set_message_code(0);

    for (uint32_t item_id : item_ids) {
        response.add_item_ids(item_id);
    }
    for (uint32_t count : item_counts) {
        response.add_item_counts(count);
    }
    response.set_exp_gain(exp_gain);
    response.set_gold_gain(gold_gain);
    response.set_remaining_count(remaining_count);

    // 序列化并发送
    std::vector<uint8_t> buffer(response.ByteSizeLong());
    response.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_SWEEP_DUNGEON_RESPONSE, buffer);
    conn->Send(packet);

    spdlog::info("[DungeonHandler] Sent SweepDungeonResponse: {} items, {} exp, {} gold, {} remaining",
        item_ids.size(), exp_gain, gold_gain, remaining_count);
}

// ========== 通知 ==========

void DungeonHandler::SendDungeonRoomCreatedNotification(
    Core::Network::Connection::Ptr conn,
    const DungeonRoom& room) {

    murim::DungeonRoomCreatedNotification notification;

    murim::DungeonRoom* room_msg = notification.mutable_room();
    room_msg->set_room_id(room.room_id);
    room_msg->set_dungeon_id(room.dungeon_id);
    room_msg->set_difficulty(room.difficulty);
    room_msg->set_created_time(room.created_time);
    room_msg->set_expire_time(room.expire_time);
    room_msg->set_started(room.started);

    murim::DungeonTeam* team_msg = room_msg->mutable_team();
    team_msg->set_team_id(room.team.team_id);
    team_msg->set_leader_id(room.team.leader_id);
    for (uint64_t member_id : room.team.member_ids) {
        team_msg->add_member_ids(member_id);
    }
    team_msg->set_max_members(room.team.max_members);
    team_msg->set_ready_count(room.team.ready_count);
    team_msg->set_all_ready(room.team.all_ready);

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_ROOM_CREATED_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent DungeonRoomCreatedNotification for room {}", room.room_id);
}

void DungeonHandler::SendDungeonRoomUpdatedNotification(
    Core::Network::Connection::Ptr conn,
    const DungeonRoom& room) {

    murim::DungeonRoomUpdatedNotification notification;

    murim::DungeonRoom* room_msg = notification.mutable_room();
    room_msg->set_room_id(room.room_id);
    room_msg->set_dungeon_id(room.dungeon_id);
    room_msg->set_difficulty(room.difficulty);
    room_msg->set_created_time(room.created_time);
    room_msg->set_expire_time(room.expire_time);
    room_msg->set_started(room.started);

    murim::DungeonTeam* team_msg = room_msg->mutable_team();
    team_msg->set_team_id(room.team.team_id);
    team_msg->set_leader_id(room.team.leader_id);
    for (uint64_t member_id : room.team.member_ids) {
        team_msg->add_member_ids(member_id);
    }
    team_msg->set_max_members(room.team.max_members);
    team_msg->set_ready_count(room.team.ready_count);
    team_msg->set_all_ready(room.team.all_ready);

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_ROOM_UPDATED_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent DungeonRoomUpdatedNotification for room {}", room.room_id);
}

void DungeonHandler::SendDungeonMemberJoinedNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t member_id,
    const std::string& member_name) {

    murim::DungeonMemberJoinedNotification notification;
    notification.set_member_id(member_id);
    notification.set_member_name(member_name);

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_MEMBER_JOINED_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent DungeonMemberJoinedNotification for member {}", member_id);
}

void DungeonHandler::SendDungeonMemberLeftNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t member_id,
    const std::string& member_name) {

    murim::DungeonMemberLeftNotification notification;
    notification.set_member_id(member_id);
    notification.set_member_name(member_name);

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_MEMBER_LEFT_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::debug("[DungeonHandler] Sent DungeonMemberLeftNotification for member {}", member_id);
}

void DungeonHandler::SendDungeonStartedNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    uint32_t dungeon_id,
    const std::vector<uint64_t>& member_ids) {

    murim::DungeonStartedNotification notification;
    notification.set_room_id(room_id);
    notification.set_dungeon_id(dungeon_id);

    for (uint64_t member_id : member_ids) {
        notification.add_member_ids(member_id);
    }

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_STARTED_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::info("[DungeonHandler] Sent DungeonStartedNotification for room {}", room_id);
}

void DungeonHandler::SendDungeonCompletedNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    uint32_t dungeon_id,
    murim::DungeonDifficulty difficulty,
    uint32_t clear_time,
    bool first_clear,
    const std::vector<uint32_t>& reward_item_ids,
    const std::vector<uint32_t>& reward_counts) {

    murim::DungeonCompletedNotification notification;
    notification.set_room_id(room_id);
    notification.set_dungeon_id(dungeon_id);
    notification.set_difficulty(difficulty);
    notification.set_clear_time(clear_time);
    notification.set_first_clear(first_clear);

    for (uint32_t item_id : reward_item_ids) {
        notification.add_reward_item_ids(item_id);
    }
    for (uint32_t count : reward_counts) {
        notification.add_reward_counts(count);
    }

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_COMPLETED_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::info("[DungeonHandler] Sent DungeonCompletedNotification for room {}, clear time: {}s, first clear: {}",
        room_id, clear_time, first_clear);
}

void DungeonHandler::SendDungeonFailedNotification(
    Core::Network::Connection::Ptr conn,
    uint64_t room_id,
    uint32_t dungeon_id,
    const std::vector<uint64_t>& member_ids) {

    murim::DungeonFailedNotification notification;
    notification.set_room_id(room_id);
    notification.set_dungeon_id(dungeon_id);

    for (uint64_t member_id : member_ids) {
        notification.add_member_ids(member_id);
    }

    // 序列化并发送
    std::vector<uint8_t> buffer(notification.ByteSizeLong());
    notification.SerializeToArray(buffer.data(), buffer.size());

    Core::Network::Packet packet(MSG_DUNGEON_FAILED_NOTIFICATION, buffer);
    conn->Send(packet);

    spdlog::info("[DungeonHandler] Sent DungeonFailedNotification for room {}", room_id);
}

} // namespace Game
} // namespace Murim
