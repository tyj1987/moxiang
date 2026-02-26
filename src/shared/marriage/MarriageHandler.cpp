#include "MarriageHandler.hpp"
#include "core/network/ByteBuffer.hpp"
#include "core/spdlog_wrapper.hpp"

namespace Murim {
namespace Game {

// ========== 消息处理器 ==========

void MarriageHandler::HandleSendProposalRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::SendProposalRequest& request) {

    spdlog::info("[MarriageHandler] Character {} sending proposal to character {} with ring {}",
        character_id, request.target_id(), request.ring_item_id());

    // 发送求婚
    bool success = MarriageManager::Instance().SendProposal(
        character_id,
        request.target_id(),
        request.ring_item_id(),
        request.message()
    );

    // TODO: 发送响应
}

void MarriageHandler::HandleRespondProposalRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::RespondProposalRequest& request) {

    spdlog::info("[MarriageHandler] Character {} responding to proposal {}, accept: {}",
        character_id, request.proposal_id(), request.accept());

    // 响应求婚
    bool success = MarriageManager::Instance().RespondProposal(
        request.proposal_id(),
        request.accept()
    );

    // TODO: 发送响应和通知
}

void MarriageHandler::HandleHoldWeddingRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::HoldWeddingRequest& request) {

    spdlog::info("[MarriageHandler] Character {} holding wedding with partner {}, type: {}",
        character_id, request.partner_id(), static_cast<int>(request.wedding_type()));

    uint64_t marriage_id;
    bool success = MarriageManager::Instance().HoldWedding(
        request.partner_id(),
        request.wedding_type(),
        request.venue_id(),
        marriage_id
    );

    // TODO: 发送响应
}

void MarriageHandler::HandleUseCoupleSkillRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::UseCoupleSkillRequest& request) {

    spdlog::info("[MarriageHandler] Character {} using couple skill {} with partner {}",
        character_id, static_cast<int>(request.skill_type()), request.partner_id());

    bool success = MarriageManager::Instance().UseCoupleSkill(
        character_id,
        request.skill_type(),
        request.partner_id()
    );

    // TODO: 发送响应
}

void MarriageHandler::HandleAddIntimacyRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::AddIntimacyRequest& request) {

    spdlog::debug("[MarriageHandler] Character {} adding {} intimacy with partner {}",
        character_id, request.amount(), request.partner_id());

    bool success = MarriageManager::Instance().AddIntimacy(
        character_id,
        request.partner_id(),
        request.amount()
    );

    // TODO: 发送响应
}

void MarriageHandler::HandleDivorceRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::DivorceRequest& request) {

    spdlog::info("[MarriageHandler] Character {} divorcing partner {}, mutual: {}, reason: {}",
        character_id, request.partner_id(), request.mutual(), request.reason());

    bool success = MarriageManager::Instance().Divorce(
        character_id,
        request.partner_id(),
        request.mutual(),
        request.reason()
    );

    // TODO: 发送响应和通知
}

void MarriageHandler::HandleGetMarriageInfoRequest(
    Core::Network::Connection::Ptr conn,
    uint64_t character_id,
    const murim::GetMarriageInfoRequest& request) {

    spdlog::debug("[MarriageHandler] Character {} requesting marriage info", character_id);

    // 加载婚姻信息
    MarriageManager::Instance().LoadMarriageInfo(character_id);

    // TODO: 发送响应
}

} // namespace Game
} // namespace Murim
