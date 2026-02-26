#include "PetHandler.hpp"
#include "PetManager.hpp"
#include "core/network/PacketSerializer.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== Helper Functions ==========

static void SendResponse(
    std::shared_ptr<Core::Network::Connection> conn,
    uint16_t message_id,
    const google::protobuf::Message& response) {

    if (!conn) {
        spdlog::error("[PetHandler] Connection is null");
        return;
    }

    // 序列化响应
    std::vector<uint8_t> data(response.ByteSizeLong());
    if (!response.SerializeToArray(data.data(), data.size())) {
        spdlog::error("[PetHandler] Failed to serialize response: message_id={:#x}", message_id);
        return;
    }

    // 使用 PacketSerializer 创建数据包
    auto buffer = Core::Network::PacketSerializer::Serialize(message_id, data);
    if (!buffer) {
        spdlog::error("[PetHandler] Failed to create packet: message_id={:#x}", message_id);
        return;
    }

    // 发送响应
    conn->AsyncSend(buffer, [message_id](const boost::system::error_code& ec, size_t bytes_sent) {
        if (ec) {
            spdlog::error("[PetHandler] Failed to send response 0x{:04X}: {}", message_id, ec.message());
        } else {
            spdlog::debug("[PetHandler] Sent response: message_id={:#x}, size={}", message_id, bytes_sent);
        }
    });
}

static uint64_t GetCharacterIdFromConnection(std::shared_ptr<Core::Network::Connection> conn) {
    // TODO: 从 Session 中获取 character_id
    // 暂时返回测试值
    return 1001;
}

// ========== Message Handlers ==========

void PetHandler::HandleGetPetListRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[PetHandler] HandleGetPetListRequest");

    // 解析请求
    murim::GetPetListRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[PetHandler] Failed to parse GetPetListRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取宠物列表
    auto& pet_manager = PetManager::Instance();
    auto pets = pet_manager.GetPetList(character_id);

    // 构建响应
    murim::GetPetListResponse response;
    response.mutable_response()->set_code(0);
    response.mutable_response()->set_message("Pet list retrieved");

    for (const auto* pet : pets) {
        auto* pet_info = response.add_pets();
        pet_info->set_pet_unique_id(pet->pet_unique_id);
        pet_info->set_pet_id(pet->pet_id);
        pet_info->set_name(pet->name);
        pet_info->set_level(pet->level);
        pet_info->set_exp(pet->exp);
        pet_info->set_hp(pet->hp);
        pet_info->set_max_hp(pet->max_hp);
        pet_info->set_attack(pet->attack);
        pet_info->set_defense(pet->defense);
        pet_info->set_speed(pet->speed);
        pet_info->set_is_active(pet->is_active);
    }

    SendResponse(conn, 0x2001, response);
}

void PetHandler::HandleSummonPetRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[PetHandler] HandleSummonPetRequest");

    // 解析请求
    murim::SummonPetRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[PetHandler] Failed to parse SummonPetRequest");
        return;
    }

    uint64_t pet_unique_id = request.pet_unique_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 召唤宠物
    auto& pet_manager = PetManager::Instance();
    bool success = pet_manager.SummonPet(character_id, pet_unique_id);

    // 构建响应
    murim::SummonPetResponse response;
    response.mutable_response()->set_code(success ? 0 : 1);
    response.mutable_response()->set_message(success ? "Pet summoned" : "Failed to summon pet");

    if (success) {
        auto* pet = pet_manager.GetPet(character_id, pet_unique_id);
        if (pet) {
            response.mutable_pet()->set_pet_unique_id(pet->pet_unique_id);
            response.mutable_pet()->set_pet_id(pet->pet_id);
            response.mutable_pet()->set_name(pet->name);
            response.mutable_pet()->set_level(pet->level);
            response.mutable_pet()->set_hp(pet->hp);
            response.mutable_pet()->set_max_hp(pet->max_hp);
            response.mutable_pet()->set_attack(pet->attack);
            response.mutable_pet()->set_defense(pet->defense);
            response.mutable_pet()->set_speed(pet->speed);
        }
    }

    SendResponse(conn, 0x2003, response);
}

void PetHandler::HandleReleasePetRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[PetHandler] HandleReleasePetRequest");

    // 解析请求
    murim::ReleasePetRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[PetHandler] Failed to parse ReleasePetRequest");
        return;
    }

    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 解散宠物
    auto& pet_manager = PetManager::Instance();
    bool success = pet_manager.DismissPet(character_id);

    // 构建响应
    murim::ReleasePetResponse response;
    response.mutable_response()->set_code(success ? 0 : 1);
    response.mutable_response()->set_message(success ? "Pet dismissed" : "Failed to dismiss pet");

    SendResponse(conn, 0x2005, response);
}

void PetHandler::HandleFeedPetRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[PetHandler] HandleFeedPetRequest");

    // 解析请求
    murim::FeedPetRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[PetHandler] Failed to parse FeedPetRequest");
        return;
    }

    uint64_t pet_unique_id = request.pet_unique_id();
    uint32_t food_id = request.food_id();
    uint32_t food_count = request.count();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 喂食宠物
    auto& pet_manager = PetManager::Instance();
    bool success = pet_manager.FeedPet(character_id, pet_unique_id, food_id, food_count);

    // 构建响应
    murim::FeedPetResponse response;
    response.set_pet_unique_id(pet_unique_id);
    response.mutable_response()->set_code(success ? 0 : 1);
    response.mutable_response()->set_message(success ? "Pet fed" : "Failed to feed pet");

    if (success) {
        // TODO: 计算实际增加的经验和亲密度
        response.set_added_exp(food_count * 100);  // 示例值
        response.set_added_intimacy(food_count * 5);  // 示例值
    }

    SendResponse(conn, 0x2007, response);
}

void PetHandler::HandleLevelUpPetRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[PetHandler] HandleLevelUpPetRequest");

    // 解析请求
    murim::LevelUpPetRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[PetHandler] Failed to parse LevelUpPetRequest");
        return;
    }

    uint64_t pet_unique_id = request.pet_unique_id();
    uint32_t exp_amount = request.exp_amount();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 获取当前等级
    auto& pet_manager = PetManager::Instance();
    auto* pet_before = pet_manager.GetPet(character_id, pet_unique_id);
    uint32_t old_level = pet_before ? pet_before->level : 0;

    // 升级宠物（使用经验道具）
    bool success = pet_manager.AddPetExp(character_id, pet_unique_id, exp_amount);

    // 构建响应
    murim::LevelUpPetResponse response;
    response.set_pet_unique_id(pet_unique_id);
    response.set_old_level(old_level);

    if (success) {
        auto* pet = pet_manager.GetPet(character_id, pet_unique_id);
        if (pet) {
            response.set_new_level(pet->level);
            response.set_current_exp(pet->exp);
        }
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("Pet leveled up");
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("Failed to level up pet");
    }

    SendResponse(conn, 0x2009, response);
}

void PetHandler::HandleEvolvePetRequest(
    std::shared_ptr<Core::Network::Connection> conn,
    const Core::Network::ByteBuffer& buffer) {

    spdlog::info("[PetHandler] HandleEvolvePetRequest");

    // 解析请求
    murim::EvolvePetRequest request;
    if (!request.ParseFromArray(buffer.GetData() + 2, buffer.GetSize() - 2)) {
        spdlog::error("[PetHandler] Failed to parse EvolvePetRequest");
        return;
    }

    uint64_t pet_unique_id = request.pet_unique_id();
    uint64_t character_id = GetCharacterIdFromConnection(conn);

    // 进化宠物
    auto& pet_manager = PetManager::Instance();

    // 获取进化前的宠物ID
    uint32_t old_pet_id = 0;
    auto* pet_before = pet_manager.GetPet(character_id, pet_unique_id);
    if (pet_before) {
        old_pet_id = pet_before->pet_id;
    }

    bool success = pet_manager.EvolvePet(character_id, pet_unique_id);

    // 构建响应
    murim::EvolvePetResponse response;
    response.set_pet_unique_id(pet_unique_id);
    response.set_old_pet_id(old_pet_id);

    if (success) {
        auto* pet = pet_manager.GetPet(character_id, pet_unique_id);
        if (pet) {
            response.set_new_pet_id(pet->pet_id);
            response.set_new_name(pet->name);
        }
        response.mutable_response()->set_code(0);
        response.mutable_response()->set_message("Pet evolved");
    } else {
        response.mutable_response()->set_code(1);
        response.mutable_response()->set_message("Failed to evolve pet");
    }

    SendResponse(conn, 0x200B, response);
}

} // namespace Game
} // namespace Murim
