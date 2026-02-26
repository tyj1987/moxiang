#pragma once

#include <memory>
#include "core/network/Connection.hpp"
#include "protocol/marriage.pb.h"
#include "MarriageManager.hpp"

namespace Murim {
namespace Game {

class MarriageHandler {
public:
    // ========== 消息处理器 ==========

    static void HandleSendProposalRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::SendProposalRequest& request
    );

    static void HandleRespondProposalRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::RespondProposalRequest& request
    );

    static void HandleHoldWeddingRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::HoldWeddingRequest& request
    );

    static void HandleUseCoupleSkillRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::UseCoupleSkillRequest& request
    );

    static void HandleAddIntimacyRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::AddIntimacyRequest& request
    );

    static void HandleDivorceRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::DivorceRequest& request
    );

    static void HandleGetMarriageInfoRequest(
        Core::Network::Connection::Ptr conn,
        uint64_t character_id,
        const murim::GetMarriageInfoRequest& request
    );

private:
    // ========== 消息ID定义 ==========

    static constexpr uint16_t MSG_SEND_PROPOSAL_REQUEST = 0x1801;
    static constexpr uint16_t MSG_SEND_PROPOSAL_RESPONSE = 0x1802;
    static constexpr uint16_t MSG_RESPOND_PROPOSAL_REQUEST = 0x1803;
    static constexpr uint16_t MSG_RESPOND_PROPOSAL_RESPONSE = 0x1804;
    static constexpr uint16_t MSG_HOLD_WEDDING_REQUEST = 0x1805;
    static constexpr uint16_t MSG_HOLD_WEDDING_RESPONSE = 0x1806;
    static constexpr uint16_t MSG_USE_COUPLE_SKILL_REQUEST = 0x1807;
    static constexpr uint16_t MSG_USE_COUPLE_SKILL_RESPONSE = 0x1808;
    static constexpr uint16_t MSG_ADD_INTIMACY_REQUEST = 0x1809;
    static constexpr uint16_t MSG_ADD_INTIMACY_RESPONSE = 0x180A;
    static constexpr uint16_t MSG_DIVORCE_REQUEST = 0x180B;
    static constexpr uint16_t MSG_DIVORCE_RESPONSE = 0x180C;
    static constexpr uint16_t MSG_GET_MARRIAGE_INFO_REQUEST = 0x180D;
    static constexpr uint16_t MSG_GET_MARRIAGE_INFO_RESPONSE = 0x180E;

    // 通知消息ID
    static constexpr uint16_t MSG_PROPOSAL_RECEIVED_NOTIFICATION = 0x180F;
    static constexpr uint16_t MSG_PROPOSAL_ACCEPTED_NOTIFICATION = 0x1810;
    static constexpr uint16_t MSG_PROPOSAL_REJECTED_NOTIFICATION = 0x1811;
    static constexpr uint16_t MSG_WEDDING_NOTIFICATION = 0x1812;
    static constexpr uint16_t MSG_DIVORCE_NOTIFICATION = 0x1813;
    static constexpr uint16_t MSG_INTIMACY_INCREASED_NOTIFICATION = 0x1814;
};

} // namespace Game
} // namespace Murim
