#include "MapServerManager.hpp"
#include "core/localization/LocalizationManager.hpp"
#include "core/database/MonsterDAO.hpp"
#include "../../core/spdlog_wrapper.hpp"
#include "../../shared/battle/Buff.hpp"
#include "../../shared/loot/Loot.hpp"
#include "../../shared/npc/NPCHandler.hpp"
#include "../../shared/shop/ShopHandler.hpp"
#include "../../shared/warehouse/WarehouseHandler.hpp"
#include "../../shared/social/ChatHandler.hpp"
#include "../../shared/mail/MailHandler.hpp"
#include "../../shared/market/MarketHandler.hpp"
#include "../../shared/trade/TradeHandler.hpp"
#include "../../shared/leaderboard/LeaderboardHandler.hpp"
#include "../../shared/achievement/AchievementHandler.hpp"
#include "../../shared/title/TitleHandler.hpp"
// TODO: 暂时注释，API 不匹配需要重构
// #include "../../shared/pet/PetHandler.hpp"
// #include "../../shared/mount/MountHandler.hpp"
#include "../../shared/vip/VIPHandler.hpp"
#include "../../shared/dungeon/DungeonHandler.hpp"
// TODO: 需要重构 - 使用 pqxx 而不是 libpq
// #include "../../shared/signin/SignInHandler.hpp"
// #include "../../shared/marriage/MarriageHandler.hpp"
#include <chrono>
#include <thread>
#include <cmath>   // for std::atan2
#include <cstring> // for std::memcpy
#include <random>  // for random drops

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace MapServer {

MapServerManager::MapServerManager()
    : character_manager_(Game::CharacterManager::Instance())
    , skill_manager_(Game::SkillManager::Instance())
    , item_manager_(Game::ItemManager::Instance())
    , quest_manager_(Game::QuestManager::Instance())
    , ai_manager_(Game::AIManager::Instance())
    , friend_manager_(Game::FriendManager::Instance())
    , party_manager_(Game::PartyManager::Instance())
    , chat_manager_(Game::ChatManager::Instance())
    , entity_manager_(EntityManager::Instance())
    , server_comm_manager_(ServerCommunicationManager::Instance())
    , npc_manager_(Game::NPCManager::Instance())
    , shop_manager_(Game::ShopManager::Instance())
    , warehouse_manager_(Game::WarehouseManager::Instance())
    , mail_manager_(Game::MailManager::Instance())
    , market_manager_(Game::MarketManager::Instance())
    , trade_manager_(Game::TradeManager::Instance())
    , leaderboard_manager_(Game::LeaderboardManager::Instance())
    , achievement_manager_(Game::AchievementManager::Instance())
    , title_manager_(Game::TitleManager::Instance())
    // TODO: 暂时注释，API 不匹配需要重构
    // , pet_manager_(Game::PetManager::Instance())
    // , mount_manager_(Game::MountManager::Instance())
    , vip_manager_(Game::VIPManager::Instance())
    , dungeon_manager_(Game::DungeonManager::Instance())
    // TODO: 需要重构 - 使用 pqxx 而不是 libpq
    // , signin_manager_(Game::SignInManager::Instance())
    // , marriage_manager_(Game::MarriageManager::Instance())
    // , dialogue_manager_(Game::DialogueManager::Instance())  // TODO: 待实现
{
}

MapServerManager& MapServerManager::Instance() {
    static MapServerManager instance;
    return instance;
}

bool MapServerManager::Initialize() {
    using namespace Murim::Core::Localization;

    spdlog::info("Initializing MapServerManager...");

    // 初始化本地化系统
    spdlog::info("Initializing Localization System...");
    if (!LocalizationManager::Instance().Initialize(Language::kChinese, "locales/")) {
        spdlog::warn("Failed to initialize LocalizationManager, using default translations");
    }

    // 初始化所有Manager
    try {
        // 初始化数据库连接池
        spdlog::info("{}", LocalizationManager::Instance().System("server.started"));
        std::string conn_str = "host=localhost port=5432 dbname=mh_game user=postgres";
        ConnectionPool::Instance().Initialize(conn_str, 10);
        spdlog::info("Database Connection Pool initialized");

        spdlog::info("Initializing CharacterManager...");
        character_manager_.Initialize();

        spdlog::info("Initializing SkillManager...");
        skill_manager_.Initialize();

        spdlog::info("Initializing ItemManager...");
        item_manager_.Initialize();

        spdlog::info("Initializing QuestManager...");
        quest_manager_.Initialize();

        spdlog::info("Initializing AIManager...");
        ai_manager_.Initialize();

        spdlog::info("Initializing FriendManager...");
        friend_manager_.Initialize();

        spdlog::info("Initializing PartyManager...");
        party_manager_.Initialize();

        spdlog::info("Initializing ChatManager...");
        chat_manager_.Initialize();

        spdlog::info("Initializing ShopManager...");
        shop_manager_.Initialize();

        spdlog::info("Initializing WarehouseManager...");
        warehouse_manager_.Initialize();

        spdlog::info("Initializing MailManager...");
        mail_manager_.Initialize();

        spdlog::info("Initializing MarketManager...");
        market_manager_.Initialize();

        spdlog::info("Initializing TradeManager...");
        trade_manager_.Initialize();

        spdlog::info("Initializing LeaderboardManager...");
        leaderboard_manager_.Initialize();

        spdlog::info("Initializing AchievementManager...");
        achievement_manager_.Initialize();

        spdlog::info("Initializing TitleManager...");
        title_manager_.Initialize();

        // TODO: 暂时注释，API 不匹配需要重构
        // spdlog::info("Initializing PetManager...");
        // pet_manager_.Initialize();
        //
        // spdlog::info("Initializing MountManager...");
        // mount_manager_.Initialize();

        spdlog::info("Initializing VIPManager...");
        vip_manager_.Initialize();

        spdlog::info("Initializing DungeonManager...");
        dungeon_manager_.Initialize();

        // TODO: 需要重构 - 使用 pqxx 而不是 libpq
        // spdlog::info("Initializing SignInManager...");
        // signin_manager_.Initialize(db_pool_);
        //
        // spdlog::info("Initializing MarriageManager...");
        // marriage_manager_.Initialize(db_pool_);

        // 初始化实体管理器 (空间索引)
        spdlog::info("Initializing EntityManager...");
        // 定义游戏世界边界 (10000 x 10000 的地图)
        // 对应 legacy: 地图坐标系
        BoundingBox world_bounds(0.0f, 0.0f, 10000.0f, 10000.0f);
        if (!entity_manager_.Initialize(world_bounds)) {
            spdlog::error("Failed to initialize EntityManager!");
            return false;
        }

        // 初始化刷怪点管理器
        spdlog::info("Initializing SpawnPointManager...");
        if (!spawn_point_manager_.Initialize(map_id_, world_bounds)) {
            spdlog::error("Failed to initialize SpawnPointManager!");
            return false;
        }

        // 初始化服务器通信管理器
        spdlog::info("Initializing ServerCommunicationManager...");
        if (!server_comm_manager_.Initialize()) {
            spdlog::error("Failed to initialize ServerCommunicationManager!");
            return false;
        }

        // 初始化网络层
        spdlog::info("Initializing Network Layer...");
        io_context_ = std::make_unique<Core::Network::IOContext>();  // 使用默认参数，不在内部启动线程
        spdlog::info("Network Layer initialized");

        // 加载怪物系统
        spdlog::info("Loading Monster System...");
        if (!LoadMonsterSystem()) {
            spdlog::warn("Failed to load monster system, continuing without monsters");
        }

        spdlog::info("MapServerManager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize MapServerManager: {}", e.what());
        return false;
    }
}

bool MapServerManager::Start() {
    if (status_ != ServerStatus::kStopped) {
        spdlog::warn("MapServer is not in stopped state, current status: {}", static_cast<int>(status_));
        return false;
    }

    spdlog::info("Starting MapServer...");
    status_ = ServerStatus::kRunning;

    // 启动服务器通信管理器
    if (!server_comm_manager_.Start()) {
        spdlog::warn("Failed to start ServerCommunicationManager (will retry)");
    }

    // 启动网络监听
    spdlog::info("Starting network acceptor on port {}...", listen_port_);
    acceptor_ = std::make_unique<Core::Network::Acceptor>(
        *io_context_,
        "0.0.0.0",  // 监听所有接口
        listen_port_
    );

    // 启动接受器并设置连接处理回调
    acceptor_->Start([this](Core::Network::Connection::Ptr conn) {
        HandleClientConnection(conn);
    });

    spdlog::info("Network acceptor started on port {}", listen_port_);

    // 启动IO线程
    io_running_ = true;

    spdlog::info("[DEBUG] Creating work guard in main thread...");
    // Create work guard in MAIN thread BEFORE starting IO thread
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    work_guard_ = std::make_unique<WorkGuard>(io_context_->GetIOContext().get_executor());
    spdlog::info("[DEBUG] Work guard created successfully");

    io_thread_ = std::thread([this]() {
        fprintf(stderr, "\n=== IO THREAD STARTED ===\n");
        fflush(stderr);

        auto& ioc = io_context_->GetIOContext();

        fprintf(stderr, "[DEBUG] io_context stopped: %s\n", ioc.stopped() ? "YES" : "NO");
        fflush(stderr);

        try {
            fprintf(stderr, "[DEBUG] About to call io_context.run()...\n");
            fflush(stderr);

            auto count = ioc.run();

            fprintf(stderr, "[DEBUG] io_context.run() returned! Executed %zu handlers\n", count);
            fflush(stderr);

        } catch (const std::exception& e) {
            fprintf(stderr, "[ERROR] Exception: %s\n", e.what());
            fflush(stderr);
        }

        fprintf(stderr, "=== IO THREAD STOPPED ===\n\n");
        fflush(stderr);
    });

    spdlog::info("[DEBUG] IO thread started");

    spdlog::info("MapServer started successfully (ServerID: {}, MapID: {})", server_id_, map_id_);
    return true;
}

void MapServerManager::Stop() {
    if (status_ != ServerStatus::kRunning) {
        return;
    }

    spdlog::info("Stopping MapServer...");
    status_ = ServerStatus::kStopping;

    // 停止网络监听
    if (acceptor_) {
        acceptor_->Stop();
    }

    // Reset work guard FIRST to allow io_context to run out of work
    spdlog::info("[DEBUG] Resetting work guard...");
    work_guard_.reset();
    spdlog::info("[DEBUG] Work guard reset");

    // 停止 IO Context
    if (io_context_) {
        io_context_->Stop();
    }

    // 等待IO线程结束
    io_running_ = false;
    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    // 停止服务器通信管理器
    server_comm_manager_.Stop();

    // 保存所有玩家数据
    for (const auto& [character_id, session_id] : players_) {
        OnPlayerLogout(character_id);
    }

    spdlog::info("MapServer stopped successfully");
}

void MapServerManager::Shutdown() {
    spdlog::info("Shutting down MapServer...");

    if (status_ != ServerStatus::kStopped) {
        Stop();
    }

    // 停止网络监听
    if (acceptor_) {
        acceptor_->Stop();
        acceptor_.reset();
    }

    if (io_context_) {
        io_context_->Stop();
        io_context_.reset();
    }

    // 等待IO线程结束
    io_running_ = false;
    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    status_ = ServerStatus::kStopped;
    spdlog::info("MapServer shutdown complete");
}

// ========== 网络连接处理 ==========

void MapServerManager::HandleClientConnection(Core::Network::Connection::Ptr conn) {
    spdlog::info("New client connected from: {}", conn->GetRemoteAddress());

    // 设置消息处理器
    conn->SetMessageHandler([this, conn](const Core::Network::ByteBuffer& buffer) {
        HandleClientMessage(conn, buffer);
    });

    // 开始接收数据
    conn->AsyncReceive();
}

void MapServerManager::HandleClientMessage(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer) {
    if (buffer.GetSize() < 2) {
        spdlog::warn("Received empty message");
        return;
    }

    // 读取消息类型 (2字节, little-endian)
    const uint8_t* data = buffer.GetData();
    uint16_t message_type = data[0] | (data[1] << 8);

    spdlog::info("Received message type 0x{:04X} from {}", message_type, conn->GetRemoteAddress());

    // DEBUG: 输出原始消息字节
    spdlog::info("=== DEBUG: Raw message ===");
    spdlog::info("Buffer size: {}", buffer.GetSize());
    spdlog::info("Message type: 0x{:04X}", message_type);

    // 输出前30字节（十六进制和ASCII）
    int bytes_to_show = std::min(30, (int)buffer.GetSize());
    std::string hex_str, ascii_str;
    for (int i = 0; i < bytes_to_show; i++) {
        char buf[16];
        sprintf(buf, "%02X ", data[i]);
        hex_str += buf;

        // ASCII可打印字符
        if (data[i] >= 32 && data[i] <= 126) {
            ascii_str += data[i];
        } else {
            ascii_str += '.';
        }

        // 每16字节换行
        if ((i + 1) % 16 == 0) {
            spdlog::info("{:04X}: {:<48} {}", i-15, hex_str, ascii_str);
            hex_str.clear();
            ascii_str.clear();
        }
    }
    if (!hex_str.empty()) {
        spdlog::info("{:04X}: {:<48} {}", bytes_to_show - (bytes_to_show % 16), hex_str, ascii_str);
    }
    spdlog::info("=== END DEBUG ===");

    // 处理各种客户端消息
    switch (message_type) {
        case 0x0020:  // PlayerLoginRequest (matching client protocol)
            HandlePlayerLogin(conn, buffer);
            break;
        case 0x000F:  // MoveRequest (matching client protocol)
            HandleMoveRequest(conn, buffer);
            break;
        case 0x0015:  // SkillCastRequest (matching client protocol)
            HandleSkillCastRequest(conn, buffer);
            break;
        case 0x0601:  // NPCInteractRequest
            Game::NPCHandler::HandleNPCInteractRequest(conn, buffer);
            break;
        case 0x0801:  // StartDialogueRequest
            Game::NPCHandler::HandleStartDialogueRequest(conn, buffer);
            break;
        case 0x0803:  // SelectDialogueOptionRequest
            Game::NPCHandler::HandleSelectDialogueOptionRequest(conn, buffer);
            break;
        case 0x0805:  // EndDialogueNotify
            Game::NPCHandler::HandleEndDialogueNotify(conn, buffer);
            break;
        case 0x0701:  // OpenShopRequest
            Game::ShopHandler::HandleOpenShopRequest(conn, buffer);
            break;
        case 0x0703:  // BuyItemRequest
            Game::ShopHandler::HandleBuyItemRequest(conn, buffer);
            break;
        case 0x0705:  // SellItemRequest
            Game::ShopHandler::HandleSellItemRequest(conn, buffer);
            break;
        case 0x0707:  // CloseShopNotify
            Game::ShopHandler::HandleCloseShopNotify(conn, buffer);
            break;
        case 0x0901:  // OpenWarehouseRequest
            Game::WarehouseHandler::HandleOpenWarehouseRequest(conn, buffer);
            break;
        case 0x0903:  // DepositItemRequest
            Game::WarehouseHandler::HandleDepositItemRequest(conn, buffer);
            break;
        case 0x0905:  // WithdrawItemRequest
            Game::WarehouseHandler::HandleWithdrawItemRequest(conn, buffer);
            break;
        case 0x0907:  // SortWarehouseRequest
            Game::WarehouseHandler::HandleSortWarehouseRequest(conn, buffer);
            break;
        case 0x090B:  // CloseWarehouseNotify (Note: using 0x09 for now, actual is 0x0B)
            Game::WarehouseHandler::HandleCloseWarehouseNotify(conn, buffer);
            break;

        // ========== 聊天系统消息 ==========
        case 0x0001:  // SendMessageRequest
            Game::ChatHandler::HandleSendMessageRequest(conn, buffer);
            break;
        case 0x0003:  // ChatHistoryRequest
            Game::ChatHandler::HandleChatHistoryRequest(conn, buffer);
            break;
        case 0x0005:  // JoinChannelRequest
            Game::ChatHandler::HandleJoinChannelRequest(conn, buffer);
            break;
        case 0x0007:  // LeaveChannelRequest
            Game::ChatHandler::HandleLeaveChannelRequest(conn, buffer);
            break;
        case 0x0009:  // ChannelMemberListRequest
            Game::ChatHandler::HandleChannelMemberListRequest(conn, buffer);
            break;

        // ========== 邮件系统消息 ==========
        case 0x0D01:  // SendMailRequest
            Game::MailHandler::HandleSendMailRequest(conn, buffer);
            break;
        case 0x0D03:  // ReadMailRequest
            Game::MailHandler::HandleReadMailRequest(conn, buffer);
            break;
        case 0x0D05:  // TakeAttachmentRequest
            Game::MailHandler::HandleTakeAttachmentRequest(conn, buffer);
            break;
        case 0x0D07:  // DeleteMailRequest
            Game::MailHandler::HandleDeleteMailRequest(conn, buffer);
            break;
        case 0x0D09:  // GetMailListRequest
            Game::MailHandler::HandleGetMailListRequest(conn, buffer);
            break;

        // ========== 市场系统消息 ==========
        case 0x0E01:  // ListItemRequest
            Game::MarketHandler::HandleListItemRequest(conn, buffer);
            break;
        case 0x0E03:  // DelistItemRequest
            Game::MarketHandler::HandleDelistItemRequest(conn, buffer);
            break;
        case 0x0E05:  // BuyItemRequest
            Game::MarketHandler::HandleBuyItemRequest(conn, buffer);
            break;
        case 0x0E07:  // BidItemRequest
            Game::MarketHandler::HandleBidItemRequest(conn, buffer);
            break;
        case 0x0E09:  // SearchMarketRequest
            Game::MarketHandler::HandleSearchMarketRequest(conn, buffer);
            break;
        case 0x0E0B:  // GetMyListingsRequest
            Game::MarketHandler::HandleGetMyListingsRequest(conn, buffer);
            break;
        case 0x0E0D:  // GetListingDetailsRequest
            Game::MarketHandler::HandleGetListingDetailsRequest(conn, buffer);
            break;

        // ========== 交易系统消息 ==========
        case 0x0F01:  // RequestTradeRequest
            Game::TradeHandler::HandleRequestTradeRequest(conn, buffer);
            break;
        case 0x0F03:  // RespondTradeRequest
            Game::TradeHandler::HandleRespondTradeRequest(conn, buffer);
            break;
        case 0x0F05:  // AddTradeItemRequest
            Game::TradeHandler::HandleAddTradeItemRequest(conn, buffer);
            break;
        case 0x0F07:  // RemoveTradeItemRequest
            Game::TradeHandler::HandleRemoveTradeItemRequest(conn, buffer);
            break;
        case 0x0F09:  // AddTradeGoldRequest
            Game::TradeHandler::HandleAddTradeGoldRequest(conn, buffer);
            break;
        case 0x0F0B:  // RemoveTradeGoldRequest
            Game::TradeHandler::HandleRemoveTradeGoldRequest(conn, buffer);
            break;
        case 0x0F0D:  // ConfirmTradeRequest
            Game::TradeHandler::HandleConfirmTradeRequest(conn, buffer);
            break;
        case 0x0F0F:  // CancelTradeRequest
            Game::TradeHandler::HandleCancelTradeRequest(conn, buffer);
            break;

        // ========== 排行榜系统消息 ==========
        case 0x1001:  // GetLeaderboardRequest
            Game::LeaderboardHandler::HandleGetLeaderboardRequest(conn, buffer);
            break;
        case 0x1003:  // GetMyRankRequest
            Game::LeaderboardHandler::HandleGetMyRankRequest(conn, buffer);
            break;
        case 0x1005:  // SearchLeaderboardRequest
            Game::LeaderboardHandler::HandleSearchLeaderboardRequest(conn, buffer);
            break;

        // ========== 成就系统消息 ==========
        case 0x1101:  // GetAchievementListRequest
            Game::AchievementHandler::HandleGetAchievementListRequest(conn, buffer);
            break;
        case 0x1103:  // GetAchievementDetailRequest
            Game::AchievementHandler::HandleGetAchievementDetailRequest(conn, buffer);
            break;
        case 0x1105:  // GetMyAchievementRequest
            Game::AchievementHandler::HandleGetMyAchievementRequest(conn, buffer);
            break;
        case 0x1107:  // ClaimAchievementRewardRequest
            Game::AchievementHandler::HandleClaimAchievementRewardRequest(conn, buffer);
            break;

        // ========== 称号系统消息 ==========
        case 0x1201:  // GetTitleListRequest
            Game::TitleHandler::HandleGetTitleListRequest(conn, buffer);
            break;
        case 0x1203:  // GetTitleDetailRequest
            Game::TitleHandler::HandleGetTitleDetailRequest(conn, buffer);
            break;
        case 0x1205:  // GetMyTitlesRequest
            Game::TitleHandler::HandleGetMyTitlesRequest(conn, buffer);
            break;
        case 0x1207:  // EquipTitleRequest
            Game::TitleHandler::HandleEquipTitleRequest(conn, buffer);
            break;
        case 0x1209:  // UnequipTitleRequest
            Game::TitleHandler::HandleUnequipTitleRequest(conn, buffer);
            break;

        // TODO: 暂时注释，API 不匹配需要重构
        // // ========== 宠物系统消息 ==========
        // case 0x1301:  // GetPetListRequest
        //     Game::PetHandler::HandleGetPetListRequest(conn, buffer);
        //     break;
        // case 0x1303:  // GetPetDetailRequest
        //     Game::PetHandler::HandleGetPetDetailRequest(conn, buffer);
        //     break;
        // case 0x1305:  // SummonPetRequest
        //     Game::PetHandler::HandleSummonPetRequest(conn, buffer);
        //     break;
        // case 0x1307:  // ReleasePetRequest
        //     Game::PetHandler::HandleReleasePetRequest(conn, buffer);
        //     break;
        // case 0x1309:  // RenamePetRequest
        //     Game::PetHandler::HandleRenamePetRequest(conn, buffer);
        //     break;
        // case 0x130B:  // LevelUpPetRequest
        //     Game::PetHandler::HandleLevelUpPetRequest(conn, buffer);
        //     break;
        // case 0x130D:  // TrainPetRequest
        //     Game::PetHandler::HandleTrainPetRequest(conn, buffer);
        //     break;
        // case 0x130F:  // EvolvePetRequest
        //     Game::PetHandler::HandleEvolvePetRequest(conn, buffer);
        //     break;
        // case 0x1311:  // LearnPetSkillRequest
        //     Game::PetHandler::HandleLearnPetSkillRequest(conn, buffer);
        //     break;
        // case 0x1313:  // EquipPetItemRequest
        //     Game::PetHandler::HandleEquipPetItemRequest(conn, buffer);
        //     break;
        // case 0x1315:  // UnequipPetItemRequest
        //     Game::PetHandler::HandleUnequipPetItemRequest(conn, buffer);
        //     break;
        // case 0x1317:  // FeedPetRequest
        //     Game::PetHandler::HandleFeedPetRequest(conn, buffer);
        //     break;
        //
        // // ========== 坐骑系统消息 ==========
        // case 0x1401:  // GetMountListRequest
        //     Game::MountHandler::HandleGetMountListRequest(conn, buffer);
        //     break;
        // case 0x1403:  // GetMountDetailRequest
        //     Game::MountHandler::HandleGetMountDetailRequest(conn, buffer);
        //     break;
        // case 0x1405:  // RideMountRequest
        //     Game::MountHandler::HandleRideMountRequest(conn, buffer);
        //     break;
        // case 0x1407:  // EvolveMountRequest
        //     Game::MountHandler::HandleEvolveMountRequest(conn, buffer);
        //     break;
        // case 0x1409:  // EquipMountSaddleRequest
        //     Game::MountHandler::HandleEquipMountSaddleRequest(conn, buffer);
        //     break;
        // case 0x140B:  // UnequipMountSaddleRequest
        //     Game::MountHandler::HandleUnequipMountSaddleRequest(conn, buffer);
        //     break;
        // case 0x140D:  // EquipMountDecorationRequest
        //     Game::MountHandler::HandleEquipMountDecorationRequest(conn, buffer);
        //     break;
        // case 0x140F:  // UnequipMountDecorationRequest
        //     Game::MountHandler::HandleUnequipMountDecorationRequest(conn, buffer);
        //     break;

        // ========== VIP系统消息 ==========
        case 0x1501:  // GetVIPInfoRequest
            Game::VIPHandler::HandleGetVIPInfoRequest(conn, buffer);
            break;
        case 0x1503:  // ClaimVIPLevelGiftRequest
            Game::VIPHandler::HandleClaimVIPLevelGiftRequest(conn, buffer);
            break;
        case 0x1505:  // ClaimVIPDailyGiftRequest
            Game::VIPHandler::HandleClaimVIPDailyGiftRequest(conn, buffer);
            break;
        case 0x1507:  // ClaimVIPMonthlyGiftRequest
            Game::VIPHandler::HandleClaimVIPMonthlyGiftRequest(conn, buffer);
            break;
        case 0x1509:  // RechargeRequest
            Game::VIPHandler::HandleRechargeRequest(conn, buffer);
            break;
        case 0x150B:  // GetVIPPrivilegesRequest
            Game::VIPHandler::HandleGetVIPPrivilegesRequest(conn, buffer);
            break;

        // TODO: DungeonHandler 需要先添加缺失的 protobuf 消息定义
        // // ========== 副本系统消息 ==========
        // case 0x1601:  // GetDungeonListRequest
        //     Game::DungeonHandler::HandleGetDungeonListRequest(conn, buffer);
        //     break;
        // case 0x1603:  // GetDungeonDetailRequest
        //     Game::DungeonHandler::HandleGetDungeonDetailRequest(conn, buffer);
        //     break;
        // case 0x1605:  // CreateDungeonRoomRequest
        //     Game::DungeonHandler::HandleCreateDungeonRoomRequest(conn, buffer);
        //     break;
        // case 0x1607:  // JoinDungeonRoomRequest
        //     Game::DungeonHandler::HandleJoinDungeonRoomRequest(conn, buffer);
        //     break;
        // case 0x1609:  // LeaveDungeonRoomRequest
        //     Game::DungeonHandler::HandleLeaveDungeonRoomRequest(conn, buffer);
        //     break;
        // case 0x160B:  // ReadyRequest
        //     Game::DungeonHandler::HandleReadyRequest(conn, buffer);
        //     break;
        // case 0x160D:  // StartDungeonRequest
        //     Game::DungeonHandler::HandleStartDungeonRequest(conn, buffer);
        //     break;
        // case 0x160F:  // SweepDungeonRequest
        //     Game::DungeonHandler::HandleSweepDungeonRequest(conn, buffer);
        //     break;

        // TODO: 需要重构 - 使用 pqxx 而不是 libpq
        //     break;
        // case 0x1803:  // RespondProposalRequest
        //     Game::MarriageHandler::HandleRespondProposalRequest(conn, 0, murim::RespondProposalRequest());
        //     break;
        // case 0x1805:  // HoldWeddingRequest
        //     Game::MarriageHandler::HandleHoldWeddingRequest(conn, 0, murim::HoldWeddingRequest());
        //     break;
        // case 0x1807:  // UseCoupleSkillRequest
        //     Game::MarriageHandler::HandleUseCoupleSkillRequest(conn, 0, murim::UseCoupleSkillRequest());
        //     break;
        // case 0x1809:  // AddIntimacyRequest
        //     Game::MarriageHandler::HandleAddIntimacyRequest(conn, 0, murim::AddIntimacyRequest());
        //     break;
        // case 0x180B:  // DivorceRequest
        //     Game::MarriageHandler::HandleDivorceRequest(conn, 0, murim::DivorceRequest());
        //     break;
        // case 0x180D:  // GetMarriageInfoRequest
        //     Game::MarriageHandler::HandleGetMarriageInfoRequest(conn, 0, murim::GetMarriageInfoRequest());
        //     break;

        default:
            spdlog::warn("Unknown message type: 0x{:04X}", message_type);
            break;
    }
}

void MapServerManager::HandlePlayerLogin(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("Handling PlayerLoginRequest");

    // 消息格式: [Header:12bytes][CharacterID:8bytes]
    if (buffer.GetSize() < 20) {
        spdlog::error("Invalid PlayerLoginRequest: size too small (got {})", buffer.GetSize());
        return;
    }

    const uint8_t* data = buffer.GetData() + 12;  // 跳过 Header

    // 读取 CharacterID (8 bytes little-endian)
    uint64_t character_id = static_cast<uint64_t>(data[0]) |
                            (static_cast<uint64_t>(data[1]) << 8) |
                            (static_cast<uint64_t>(data[2]) << 16) |
                            (static_cast<uint64_t>(data[3]) << 24) |
                            (static_cast<uint64_t>(data[4]) << 32) |
                            (static_cast<uint64_t>(data[5]) << 40) |
                            (static_cast<uint64_t>(data[6]) << 48) |
                            (static_cast<uint64_t>(data[7]) << 56);

    spdlog::info("Player login request: character_id={}", character_id);

    // 加载角色数据
    auto character = character_manager_.GetCharacter(character_id);
    if (!character.has_value()) {
        spdlog::error("Character not found: {}", character_id);

        // 发送失败响应
        std::vector<uint8_t> response = {
            0x21, 0x00,              // MessageType: PlayerLoginResponse (0x21)
            0x04, 0x00, 0x00, 0x00,  // BodySize: 4
            0x01, 0x00, 0x00, 0x00,  // Sequence: 1
            0x00, 0x00,              // Reserved
            0x01, 0x00, 0x00, 0x00   // ResultCode: 1 (失败)
        };
        auto response_buffer = std::make_shared<Core::Network::ByteBuffer>(response.data(), response.size());
        conn->AsyncSend(response_buffer, [](const boost::system::error_code& ec, size_t) {
            if (ec) {
                spdlog::error("Failed to send PlayerLoginResponse: {}", ec.message());
            }
        });
        return;
    }

    // 生成会话 ID (简化版本,实际应该使用更安全的生成方式)
    uint64_t session_id = reinterpret_cast<uint64_t>(conn.get());

    // 调用 OnPlayerLogin
    if (!OnPlayerLogin(character_id, session_id, conn)) {
        spdlog::error("OnPlayerLogin failed for character_id={}", character_id);

        // 发送失败响应
        std::vector<uint8_t> response = {
            0x21, 0x00,              // MessageType: PlayerLoginResponse (0x21)
            0x04, 0x00, 0x00, 0x00,  // BodySize: 4
            0x01, 0x00, 0x00, 0x00,  // Sequence: 1
            0x00, 0x00,              // Reserved
            0x02, 0x00, 0x00, 0x00   // ResultCode: 2 (其他错误)
        };
        auto response_buffer = std::make_shared<Core::Network::ByteBuffer>(response.data(), response.size());
        conn->AsyncSend(response_buffer, [](const boost::system::error_code& ec, size_t) {
            if (ec) {
                spdlog::error("Failed to send PlayerLoginResponse: {}", ec.message());
            }
        });
        return;
    }

    // 构建成功响应
    // Response format:
    // [Header:12bytes]
    // [ResultCode:4bytes]
    // [CharacterID:8bytes]
    // [NameLength:4bytes][Name:variable]
    // [MapID:4bytes]
    // [PositionX:4bytes][PositionY:4bytes][PositionZ:4bytes]
    // [HP:4bytes][MaxHP:4bytes][MP:4bytes][MaxMP:4bytes]

    std::vector<uint8_t> body;
    const std::string& name = character->name;

    // ResultCode (成功 = 0)
    body.push_back(0x00); body.push_back(0x00); body.push_back(0x00); body.push_back(0x00);

    // CharacterID (8 bytes little-endian)
    for (int i = 0; i < 8; i++) {
        body.push_back((character_id >> (i * 8)) & 0xFF);
    }

    // NameLength (4 bytes)
    uint32_t name_len = static_cast<uint32_t>(name.length());
    for (int i = 0; i < 4; i++) {
        body.push_back((name_len >> (i * 8)) & 0xFF);
    }

    // Name
    body.insert(body.end(), name.begin(), name.end());

    // MapID (4 bytes)
    body.push_back(0x01); body.push_back(0x00); body.push_back(0x00); body.push_back(0x00);

    // Position X, Y, Z (4 bytes each, float)
    float pos[3] = { character->x, character->y, character->z };
    for (int i = 0; i < 3; i++) {
        const uint8_t* pos_bytes = reinterpret_cast<const uint8_t*>(&pos[i]);
        for (int j = 0; j < 4; j++) {
            body.push_back(pos_bytes[j]);
        }
    }

    // HP, MaxHP, MP, MaxMP (4 bytes each)
    uint32_t stats[4] = { character->hp, character->max_hp, character->mp, character->max_mp };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            body.push_back((stats[i] >> (j * 8)) & 0xFF);
        }
    }

    // 构建完整消息
    std::vector<uint8_t> message;
    // Header: MessageType (2 bytes)
    message.push_back(0x21); message.push_back(0x00);
    // BodySize (4 bytes)
    uint32_t body_size = static_cast<uint32_t>(body.size());
    for (int i = 0; i < 4; i++) {
        message.push_back((body_size >> (i * 8)) & 0xFF);
    }
    // Sequence (4 bytes)
    message.push_back(0x01); message.push_back(0x00); message.push_back(0x00); message.push_back(0x00);
    // Reserved (2 bytes)
    message.push_back(0x00); message.push_back(0x00);

    // Append body
    message.insert(message.end(), body.begin(), body.end());

    // 发送响应
    auto response_buffer = std::make_shared<Core::Network::ByteBuffer>(message.data(), message.size());
    conn->AsyncSend(response_buffer, [character_id, name](const boost::system::error_code& ec, size_t) {
        if (ec) {
            spdlog::error("Failed to send PlayerLoginResponse: {}", ec.message());
        } else {
            spdlog::info("Player login response sent: character_id={}, name={}", character_id, name);
        }
    });

    spdlog::info("Player login successful: character_id={}, name={}, session_id={}",
        character_id, name, session_id);
}

// ========== 玩家管理 ==========

bool MapServerManager::OnPlayerLogin(uint64_t character_id, uint64_t session_id, Core::Network::Connection::Ptr conn) {
    spdlog::info("Player login: character_id={}, session_id={}", character_id, session_id);

    // 加载角色数据
    auto character = character_manager_.GetCharacter(character_id);
    if (!character.has_value()) {
        using namespace Murim::Core::Localization;
        spdlog::error("{}", LocalizationManager::Instance().Error("error.character.not.found"));
        return false;
    }

    // 更新在线状态
    character_manager_.CharacterLogin(character_id);

    // 添加到在线列表
    players_[character_id] = session_id;
    sessions_[session_id] = character_id;

    // 保存Connection到映射（用于广播）
    character_connections_[character_id] = conn;
    spdlog::info("Connection saved for character_id={}, total connections: {}",
                 character_id, character_connections_.size());

    // 创建玩家实体并添加到实体管理器
    // 使用 core spatial Position (与 EntityManager 一致)
    Murim::Position player_pos(character->x, character->y, character->z);
    GameEntity player_entity(
        character_id,
        EntityType::kPlayer,
        player_pos,
        &character.value()  // 指向角色数据的指针
    );

    // TODO: 临时绕过实体添加检查，先测试地图加载流程
    // 正式版本需要修复EntityManager::AddEntity失败的根本原因
    if (!entity_manager_.AddEntity(player_entity)) {
        spdlog::warn("[TEMP FIX] Failed to add player entity: character_id={}, continuing anyway", character_id);
        // 不再返回false，允许登录继续
        // return false;  // 已注释：临时绕过
    } else {
        spdlog::info("Player entity added successfully: character_id={}", character_id);
    }

    // 通知好友玩家上线
    friend_manager_.OnCharacterLogin(character_id);

    using namespace Murim::Core::Localization;
    spdlog::info("{}", LocalizationManager::Instance().TranslateFormat("log.player.login", TextCategory::kLog,
                static_cast<long long>(0), static_cast<long long>(character_id), character->name.c_str()));

    return true;
}

void MapServerManager::OnPlayerLogout(uint64_t character_id) {
    spdlog::info("Player logout: character_id={}", character_id);

    // 从实体管理器中移除玩家实体
    if (!entity_manager_.RemoveEntity(character_id)) {
        spdlog::warn("Failed to remove player entity: character_id={}", character_id);
    }

    // 移除会话
    if (players_.find(character_id) != players_.end()) {
        uint64_t session_id = players_[character_id];
        players_.erase(character_id);
        sessions_.erase(session_id);
    }

    // 移除Connection映射
    if (character_connections_.find(character_id) != character_connections_.end()) {
        character_connections_.erase(character_id);
        spdlog::info("Connection removed for character_id={}, remaining connections: {}",
                     character_id, character_connections_.size());
    }

    // 更新离线状态
    character_manager_.CharacterLogout(character_id, 0);

    // 通知好友玩家下线
    friend_manager_.OnCharacterLogout(character_id);

    spdlog::info("Player logout successful: character_id={}", character_id);
}

void MapServerManager::HandleMoveRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("Handling MoveRequest");

    // 解析移动请求消息
    // 格式: [Header:12bytes][CharacterID:8][PosX:4][PosY:4][PosZ:4][Direction:2][Timestamp:8]
    // 总共: 12 + 8 + 4 + 4 + 4 + 2 + 8 = 42 bytes

    if (buffer.GetSize() < 42) {
        spdlog::error("MoveRequest message too small: {} bytes (expected 42)", buffer.GetSize());
        SendMoveResponse(conn, 0, 1, 0.0f, 0.0f, 0.0f, 0);  // Error: invalid message
        return;
    }

    const uint8_t* data = buffer.GetData() + 12;  // 跳过 Header

    // 解析字段
    uint64_t character_id;
    float pos_x, pos_y, pos_z;
    uint16_t direction;
    uint64_t timestamp;

    std::memcpy(&character_id, data, 8);
    std::memcpy(&pos_x, data + 8, 4);
    std::memcpy(&pos_y, data + 12, 4);
    std::memcpy(&pos_z, data + 16, 4);
    std::memcpy(&direction, data + 20, 2);
    std::memcpy(&timestamp, data + 22, 8);

    spdlog::info("MoveRequest: character_id={}, pos=({:.1f}, {:.1f}, {:.1f}), direction={}, timestamp={}",
                 character_id, pos_x, pos_y, pos_z, direction, timestamp);

    // 检查玩家是否在线
    // TODO: 暂时禁用此检查以调试
    /*
    if (players_.find(character_id) == players_.end()) {
        spdlog::warn("Player not online: character_id={}", character_id);
        SendMoveResponse(conn, character_id, 1, pos_x, pos_y, pos_z, 0);  // Error: player not online
        return;
    }
    */
    spdlog::info("Players map size: {}", players_.size());
    for (const auto& pair : players_) {
        spdlog::info("  Player: char_id={}, session_id={}", pair.first, pair.second);
    }

    // 获取当前角色位置
    auto character = character_manager_.GetCharacter(character_id);
    if (!character) {
        spdlog::error("Character not found: character_id={}", character_id);
        SendMoveResponse(conn, character_id, 2, pos_x, pos_y, pos_z, 0);  // Error: character not found
        return;
    }

    // 构造移动信息
    Murim::Game::MovementInfo move_info;
    move_info.character_id = character_id;
    move_info.map_id = character->map_id;
    move_info.position = Murim::Game::Position(character->x, character->y, character->z);
    move_info.direction = direction;
    move_info.move_state = 2;  // 默认奔跑
    move_info.timestamp = timestamp;
    move_info.last_valid_x = character->x;
    move_info.last_valid_y = character->y;
    move_info.last_update_time = timestamp - 100;  // 假设100ms前更新过

    // 新位置
    Murim::Game::Position new_position(pos_x, pos_y, pos_z);

    // 计算时间差 (秒)
    // 使用系统时间 (Unix 时间戳)，与客户端保持一致
    auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    int64_t time_delta_ms = current_time - timestamp;
    float delta_time = time_delta_ms / 1000.0f;  // 转换为秒

    // 防止除零或负数时间差
    if (delta_time <= 0.0f || delta_time > 5.0f) {
        spdlog::warn("Invalid time delta: {} ms, using default 0.1s", time_delta_ms);
        delta_time = 0.1f;
    }

    spdlog::info("Time delta: {} ms ({:.3f} s)", time_delta_ms, delta_time);

    // 获取角色速度
    float speed = move_info.GetSpeed();

    // TODO: 暂时禁用移动验证以测试基本功能
    /*
    // 验证移动
    Murim::Game::MovementValidationResult validation_result =
        Murim::Game::MovementManager::Instance().ValidatePositionUpdate(
            character_id,
            move_info.position,
            new_position,
            delta_time,
            speed
        );

    if (validation_result != Murim::Game::MovementValidationResult::kValid) {
        spdlog::warn("Movement validation failed: character_id={}, result={}",
                     character_id, static_cast<int>(validation_result));

        // 发送失败响应，返回旧位置
        SendMoveResponse(conn, character_id, 1, character->x, character->y, character->z, 0);
        return;
    }
    */
    spdlog::info("Movement validation skipped (testing mode)");

    // 更新位置到数据库
    bool update_success = character_manager_.UpdatePosition(
        character_id,
        pos_x,
        pos_y,
        pos_z,
        direction
    );

    if (!update_success) {
        spdlog::error("Failed to update position in database: character_id={}", character_id);
        SendMoveResponse(conn, character_id, 2, character->x, character->y, character->z, 0);
        return;
    }

    // 更新实体管理器中的位置
    // 注意: EntityManager 使用 Murim::Position (core spatial), 而不是 Murim::Game::Position (movement)
    Murim::Position core_position(pos_x, pos_y, pos_z);
    entity_manager_.UpdateEntityPosition(character_id, core_position);

    spdlog::info("Position updated: character_id={}, pos=({:.1f}, {:.1f}, {:.1f})",
                 character_id, pos_x, pos_y, pos_z);

    // 广播移动给附近玩家
    BroadcastCharacterMovement(character_id, pos_x, pos_y, pos_z, direction, conn);

    // 发送成功响应
    SendMoveResponse(conn, character_id, 0, pos_x, pos_y, pos_z, direction);
}

void MapServerManager::SendMoveResponse(Core::Network::Connection::Ptr conn, int64_t character_id, int32_t result_code, float x, float y, float z, uint16_t direction) {
    spdlog::info("Sending MoveResponse: character_id={}, result_code={}, pos=({:.1f}, {:.1f}, {:.1f}), dir={}",
                 character_id, result_code, x, y, z, direction);

    // 构造移动响应消息
    // 格式: [ResultCode:4][PosX:4][PosY:4][PosZ:4]

    std::vector<uint8_t> response;

    // ResultCode: 4 bytes
    response.push_back((result_code >> 0) & 0xFF);
    response.push_back((result_code >> 8) & 0xFF);
    response.push_back((result_code >> 16) & 0xFF);
    response.push_back((result_code >> 24) & 0xFF);

    // PosX: 4 bytes
    uint32_t pos_x_bits;
    std::memcpy(&pos_x_bits, &x, 4);
    response.push_back((pos_x_bits >> 0) & 0xFF);
    response.push_back((pos_x_bits >> 8) & 0xFF);
    response.push_back((pos_x_bits >> 16) & 0xFF);
    response.push_back((pos_x_bits >> 24) & 0xFF);

    // PosY: 4 bytes
    uint32_t pos_y_bits;
    std::memcpy(&pos_y_bits, &y, 4);
    response.push_back((pos_y_bits >> 0) & 0xFF);
    response.push_back((pos_y_bits >> 8) & 0xFF);
    response.push_back((pos_y_bits >> 16) & 0xFF);
    response.push_back((pos_y_bits >> 24) & 0xFF);

    // PosZ: 4 bytes
    uint32_t pos_z_bits;
    std::memcpy(&pos_z_bits, &z, 4);
    response.push_back((pos_z_bits >> 0) & 0xFF);
    response.push_back((pos_z_bits >> 8) & 0xFF);
    response.push_back((pos_z_bits >> 16) & 0xFF);
    response.push_back((pos_z_bits >> 24) & 0xFF);

    // 添加消息头: [MessageType:2][BodySize:4][Sequence:4][Reserved:2]
    std::vector<uint8_t> message;

    // MessageType: 0x000F (MoveResponse)
    message.push_back(0x0F);
    message.push_back(0x00);

    // BodySize: 4 bytes
    uint32_t body_size = response.size();
    message.push_back((body_size >> 0) & 0xFF);
    message.push_back((body_size >> 8) & 0xFF);
    message.push_back((body_size >> 16) & 0xFF);
    message.push_back((body_size >> 24) & 0xFF);

    // Sequence: 4 bytes (使用0，因为不需要序列号)
    message.push_back(0);
    message.push_back(0);
    message.push_back(0);
    message.push_back(0);

    // Reserved: 2 bytes
    message.push_back(0);
    message.push_back(0);

    // 添加消息体
    message.insert(message.end(), response.begin(), response.end());

    // 发送响应
    auto response_buffer = std::make_shared<Core::Network::ByteBuffer>(message.data(), message.size());
    conn->AsyncSend(response_buffer, [result_code, x, y, z](const boost::system::error_code& ec, size_t) {
        if (ec) {
            spdlog::error("Failed to send MoveResponse: {}", ec.message());
        } else {
            spdlog::info("MoveResponse sent: result_code={}, pos=({:.1f}, {:.1f}, {:.1f})",
                        result_code, x, y, z);
        }
    });
}

void MapServerManager::HandleSkillCastRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer) {
    spdlog::info("Handling SkillCastRequest");

    // 解析技能释放请求消息
    // 格式: [Header:12bytes][CharacterID:8][SkillID:4][TargetID:8][TargetX:4][TargetY:4][TargetZ:4][Direction:2][Timestamp:8]
    // 总共: 12 + 8 + 4 + 8 + 4 + 4 + 4 + 2 + 8 = 54 bytes

    if (buffer.GetSize() < 54) {
        spdlog::error("SkillCastRequest message too small: {} bytes (expected 54)", buffer.GetSize());
        SendSkillCastResponse(conn, 3, 0, 0);  // Error: invalid message
        return;
    }

    const uint8_t* data = buffer.GetData() + 12;  // 跳过 Header

    // 解析字段
    uint64_t character_id;
    uint32_t skill_id;
    uint64_t target_id;
    float target_x, target_y, target_z;
    uint16_t direction;
    uint64_t timestamp;

    std::memcpy(&character_id, data, 8);
    std::memcpy(&skill_id, data + 8, 4);
    std::memcpy(&target_id, data + 12, 8);
    std::memcpy(&target_x, data + 20, 4);
    std::memcpy(&target_y, data + 24, 4);
    std::memcpy(&target_z, data + 28, 4);
    std::memcpy(&direction, data + 32, 2);
    std::memcpy(&timestamp, data + 34, 8);

    spdlog::info("SkillCastRequest: character_id={}, skill_id={}, target_id={}, target=({:.1f}, {:.1f}, {:.1f}), direction={}, timestamp={}",
                 character_id, skill_id, target_id, target_x, target_y, target_z, direction, timestamp);

    // 检查玩家是否在线
    // TODO: 暂时禁用此检查以调试
    /*
    if (players_.find(character_id) == players_.end()) {
        spdlog::warn("Player not online: character_id={}", character_id);
        SendSkillCastResponse(conn, 4, skill_id, 0);  // Error: player not online
        return;
    }
    */

    // 获取技能信息
    auto skill_opt = skill_manager_.GetSkill(skill_id);
    if (!skill_opt) {
        spdlog::error("Skill not found: skill_id={}", skill_id);
        SendSkillCastResponse(conn, 3, skill_id, 0);  // Error: skill not found
        return;
    }

    const auto& skill = *skill_opt;

    // 获取角色信息
    auto character_opt = character_manager_.GetCharacter(character_id);
    if (!character_opt) {
        spdlog::error("Character not found: character_id={}", character_id);
        SendSkillCastResponse(conn, 4, skill_id, 0);  // Error: character not found
        return;
    }

    const auto& character = *character_opt;

    // 检查内力是否足够
    if (character.mp < skill.mp_cost) {
        spdlog::warn("Not enough MP: character_id={}, mp={}, mp_cost={}",
                     character_id, character.mp, skill.mp_cost);
        SendSkillCastResponse(conn, 2, skill_id, 0);  // Error: not enough MP
        return;
    }

    // 检查技能冷却
    uint16_t cooldown_remaining = skill_manager_.GetSkillCooldown(character_id, skill_id);
    spdlog::info("[DEBUG] Cooldown check: character_id={}, skill_id={}, remaining={}s",
                 character_id, skill_id, cooldown_remaining);
    if (cooldown_remaining > 0) {
        spdlog::warn("Skill on cooldown: character_id={}, skill_id={}, remaining={}s",
                     character_id, skill_id, cooldown_remaining);
        SendSkillCastResponse(conn, 1, skill_id, 0);  // Error: skill on cooldown
        return;
    }

    // 检查技能距离
    // TODO: 实现距离检查

    // 获取施法者属性用于伤害计算
    auto caster_stats_opt = character_manager_.GetCharacterStats(character_id);
    uint32_t attack_power = 100;  // 默认攻击力
    if (caster_stats_opt.has_value()) {
        attack_power = caster_stats_opt->physical_attack;
        if (attack_power == 0) {
            attack_power = caster_stats_opt->CalculatePhysicalAttack();
        }
    }

    // 计算伤害
    uint32_t damage = skill.base_damage;
    if (skill.damage_multiplier > 0.0f) {
        damage = static_cast<uint32_t>(skill.base_damage + (attack_power * skill.damage_multiplier));
    }

    spdlog::info("Damage calculation: base={}, attack_power={}, multiplier={}, total_damage={}",
                 skill.base_damage, attack_power, skill.damage_multiplier, damage);

    // 如果有目标，应用伤害
    if (target_id > 0) {
        spdlog::info("Target specified: target_id={}", target_id);

        // 验证目标存在
        auto target_opt = character_manager_.GetCharacter(target_id);
        if (!target_opt.has_value()) {
            spdlog::warn("Target not found: target_id={}", target_id);
            SendSkillCastResponse(conn, 4, skill_id, 0);  // Error: invalid target
            return;
        }

        auto target = target_opt.value();

        // 检查复活保护（复活后3秒无敌）
        const uint32_t REVIVE_PROTECTION_SECONDS = 3;
        if (target.IsReviveProtected(REVIVE_PROTECTION_SECONDS)) {
            uint32_t protection_remaining = target.GetReviveProtectionRemaining(REVIVE_PROTECTION_SECONDS);
            spdlog::info("Target is revive protected: target_id={}, remaining={}s, damage ignored",
                         target_id, protection_remaining);
            // 伤害被保护忽略，不扣HP
            // TODO: 可选：发送保护提示消息给攻击者
            SendSkillCastResponse(conn, 0, skill_id, 0);  // 技能释放成功，但伤害被保护
            return;
        }

        // 扣除目标HP
        uint32_t old_hp = target.hp;
        uint32_t new_hp = (damage >= old_hp) ? 0 : (old_hp - damage);

        spdlog::info("Applying damage: target_id={}, old_hp={}, damage={}, new_hp={}",
                     target_id, old_hp, damage, new_hp);

        // 更新目标HP
        target.hp = new_hp;

        // 保存到数据库
        if (!character_manager_.SaveCharacter(target)) {
            spdlog::error("Failed to save target HP: target_id={}", target_id);
        } else {
            spdlog::info("Target HP saved: target_id={}, new_hp={}", target_id, new_hp);
        }

        // 广播目标HP变化
        BroadcastCharacterStats(target_id, new_hp, target.max_hp, target.mp, target.max_mp, conn);

        // 如果目标死亡，处理死亡逻辑
        if (new_hp == 0) {
            spdlog::info("Target killed: target_id={}", target_id);
            // 处理角色死亡（killer_id=attacker，非玩家杀死）
            HandleCharacterDeath(target_id, character_id, conn);
        }

    } else {
        // 范围技能 - 查询并伤害范围内的所有目标
        spdlog::info("Area skill: querying targets in range");

        // 获取施法者位置
        Murim::Position caster_pos(character.x, character.y, character.z);

        // 确定范围半径
        float radius = skill.radius;
        if (radius <= 0.0f) {
            radius = 100.0f;  // 默认半径
        }

        spdlog::info("Area skill: caster_pos=({:.1f}, {:.1f}, {:.1f}), radius={:.1f}",
                     caster_pos.x, caster_pos.y, caster_pos.z, radius);

        // 查询范围内的所有玩家
        auto nearby_entities = entity_manager_.QueryNearbyEntities(
            caster_pos, radius, EntityType::kPlayer);

        spdlog::info("Area skill: found {} nearby entities", nearby_entities.size());

        // 对范围内的每个目标造成伤害
        int targets_hit = 0;
        int total_damage_dealt = 0;

        for (GameEntity* entity : nearby_entities) {
            // 跳过施法者自己
            if (entity->entity_id == character_id) {
                continue;
            }

            // 获取目标角色数据
            auto target_opt = character_manager_.GetCharacter(entity->entity_id);
            if (!target_opt.has_value()) {
                spdlog::warn("Area skill: target not found: target_id={}", entity->entity_id);
                continue;
            }

            auto target = target_opt.value();

            // 计算距离和角度
            float distance = caster_pos.Distance2D(Murim::Position(target.x, target.y, target.z));

            // 范围检查
            bool in_range = true;

            if (skill.area_shape == Game::SkillAreaShape::kCircle) {
                // 圆形范围：只检查距离
                in_range = (distance <= radius);
            }
            else if (skill.area_shape == Game::SkillAreaShape::kSector) {
                // 扇形范围：检查距离和角度
                if (distance > radius) {
                    in_range = false;
                } else {
                    // 计算目标相对于施法者的角度
                    float dx = target.x - caster_pos.x;
                    float dy = target.y - caster_pos.y;

                    // 使用 atan2 计算角度（返回 -π 到 π）
                    float angle_to_target = std::atan2(dy, dx);

                    // 转换为度数（0-360）
                    float angle_degrees = angle_to_target * 180.0f / M_PI;
                    if (angle_degrees < 0) {
                        angle_degrees += 360.0f;
                    }

                    // 扇形角度范围：[direction - angle/2, direction + angle/2]
                    float half_angle = skill.angle / 2.0f;
                    float angle_min = direction - half_angle;
                    float angle_max = direction + half_angle;

                    // 标准化角度到 0-360
                    auto normalize_angle = [](float angle) {
                        while (angle < 0) angle += 360.0f;
                        while (angle >= 360.0f) angle -= 360.0f;
                        return angle;
                    };

                    angle_min = normalize_angle(angle_min);
                    angle_max = normalize_angle(angle_max);

                    // 检查目标是否在扇形角度范围内
                    if (angle_min < angle_max) {
                        in_range = (angle_degrees >= angle_min && angle_degrees <= angle_max);
                    } else {
                        // 跨越0度的情况（如：350度到10度）
                        in_range = (angle_degrees >= angle_min || angle_degrees <= angle_max);
                    }

                    spdlog::debug("Sector check: target_angle={:.1f}, range=[{:.1f}, {:.1f}], in_range={}",
                                angle_degrees, angle_min, angle_max, in_range);
                }
            }
            else if (skill.area_shape == Game::SkillAreaShape::kRectangle) {
                // 矩形范围：检查目标是否在矩形内
                // 矩形参数: 宽度(skill.radius), 长度(skill.range)
                // 假设矩形沿施法者朝向(direction)延伸

                if (distance > skill.range) {
                    in_range = false;
                } else {
                    // 计算目标相对于施法者的位置
                    float dx = target.x - caster_pos.x;
                    float dy = target.y - caster_pos.y;

                    // 旋转坐标系，使direction对齐X轴正方向
                    float direction_rad = direction * M_PI / 180.0f;
                    float cos_a = std::cos(-direction_rad);
                    float sin_a = std::sin(-direction_rad);

                    // 旋转变换
                    float rotated_x = dx * cos_a - dy * sin_a;
                    float rotated_y = dx * sin_a + dy * cos_a;

                    // 矩形判定
                    // X范围: [0, skill.range]
                    // Y范围: [-skill.radius/2, skill.radius/2]
                    float half_width = skill.radius / 2.0f;
                    float length = skill.range;

                    in_range = (rotated_x >= 0 && rotated_x <= length &&
                                std::abs(rotated_y) <= half_width);

                    spdlog::debug("Rectangle check: rotated_pos=({:.1f}, {:.1f}), range=[0, {:.1f}]x[{:.1f}, {:.1f}], in_range={}",
                                rotated_x, rotated_y, 0.0f, length, -half_width, half_width, in_range);
                }
            }
            else if (skill.area_shape == Game::SkillAreaShape::kLine) {
                // 直线范围：检查目标是否在直线路径上
                // 直线参数: 宽度(skill.radius), 长度(skill.range)
                // 沿施法者朝向延伸

                if (distance > skill.range) {
                    in_range = false;
                } else {
                    // 计算目标相对于施法者的位置
                    float dx = target.x - caster_pos.x;
                    float dy = target.y - caster_pos.y;

                    // 旋转坐标系
                    float direction_rad = direction * M_PI / 180.0f;
                    float cos_a = std::cos(-direction_rad);
                    float sin_a = std::sin(-direction_rad);

                    // 旋转变换
                    float rotated_x = dx * cos_a - dy * sin_a;
                    float rotated_y = dx * sin_a + dy * cos_a;

                    // 直线判定：X在范围内，Y在宽度内
                    float half_width = skill.radius / 2.0f;
                    float length = skill.range;

                    in_range = (rotated_x >= 0 && rotated_x <= length &&
                                std::abs(rotated_y) <= half_width);

                    spdlog::debug("Line check: rotated_pos=({:.1f}, {:.1f}), range=[0, {:.1f}]x[{:.1f}, {:.1f}], in_range={}",
                                rotated_x, rotated_y, 0.0f, length, -half_width, half_width, in_range);
                }
            }
            else {
                // 未知范围形状，默认使用圆形判定
                in_range = (distance <= radius);
            }

            if (!in_range) {
                continue;  // 超出范围
            }

            // 检查复活保护（复活后3秒无敌）
            const uint32_t REVIVE_PROTECTION_SECONDS = 3;
            if (target.IsReviveProtected(REVIVE_PROTECTION_SECONDS)) {
                uint32_t protection_remaining = target.GetReviveProtectionRemaining(REVIVE_PROTECTION_SECONDS);
                spdlog::info("Area skill: target is revive protected: target_id={}, remaining={}s, damage ignored",
                             entity->entity_id, protection_remaining);
                continue;  // 跳过此目标，伤害被保护忽略
            }

            // 扣除目标HP
            uint32_t old_hp = target.hp;
            uint32_t new_hp = (damage >= old_hp) ? 0 : (old_hp - damage);

            spdlog::info("Area skill: damaging target_id={}, old_hp={}, damage={}, new_hp={}, distance={:.1f}",
                         entity->entity_id, old_hp, damage, new_hp, distance);

            // 更新目标HP
            target.hp = new_hp;

            // 保存到数据库
            if (!character_manager_.SaveCharacter(target)) {
                spdlog::error("Failed to save target HP: target_id={}", entity->entity_id);
            } else {
                spdlog::info("Target HP saved: target_id={}, new_hp={}", entity->entity_id, new_hp);
            }

            // 广播目标HP变化
            BroadcastCharacterStats(entity->entity_id, new_hp, target.max_hp, target.mp, target.max_mp, conn);

            // 如果目标死亡，处理死亡逻辑
            if (new_hp == 0) {
                spdlog::info("Area skill: target killed: target_id={}", entity->entity_id);
                // 处理角色死亡（killer_id=attacker，范围技能杀死）
                HandleCharacterDeath(entity->entity_id, character_id, conn);
            }

            targets_hit++;
            total_damage_dealt += damage;
        }

        spdlog::info("Area skill completed: targets_hit={}, total_damage_dealt={}",
                     targets_hit, total_damage_dealt);
    }

    // 扣除内力
    // TODO: 更新角色内力到数据库
    uint32_t new_mp = character.mp - skill.mp_cost;
    spdlog::info("Skill cast: skill_id={}, damage={}, mp_cost={}, new_mp={}",
                 skill_id, damage, skill.mp_cost, new_mp);

    // 广播属性变化给附近玩家（模拟MP消耗）
    BroadcastCharacterStats(character_id, character.hp, character.max_hp, new_mp, character.max_mp, conn);

    // 开始技能冷却
    skill_manager_.StartSkillCooldown(character_id, skill_id);
    spdlog::info("Skill cooldown started: character_id={}, skill_id={}, cooldown={}s",
                 static_cast<uint64_t>(character_id),
                 skill_id,
                 static_cast<uint32_t>(skill.cooldown));

    // 发送成功响应
    SendSkillCastResponse(conn, 0, skill_id, damage);
}

void MapServerManager::SendSkillCastResponse(Core::Network::Connection::Ptr conn, int32_t result_code, uint32_t skill_id, uint32_t damage) {
    spdlog::info("Sending SkillCastResponse: result_code={}, skill_id={}, damage={}",
                 result_code, skill_id, damage);

    // 构造技能释放响应消息
    // 格式: [ResultCode:4][SkillID:4][Damage:4]

    std::vector<uint8_t> response;

    // ResultCode: 4 bytes
    response.push_back((result_code >> 0) & 0xFF);
    response.push_back((result_code >> 8) & 0xFF);
    response.push_back((result_code >> 16) & 0xFF);
    response.push_back((result_code >> 24) & 0xFF);

    // SkillID: 4 bytes
    response.push_back((skill_id >> 0) & 0xFF);
    response.push_back((skill_id >> 8) & 0xFF);
    response.push_back((skill_id >> 16) & 0xFF);
    response.push_back((skill_id >> 24) & 0xFF);

    // Damage: 4 bytes
    response.push_back((damage >> 0) & 0xFF);
    response.push_back((damage >> 8) & 0xFF);
    response.push_back((damage >> 16) & 0xFF);
    response.push_back((damage >> 24) & 0xFF);

    // 添加消息头: [MessageType:2][BodySize:4][Sequence:4][Reserved:2]
    std::vector<uint8_t> message;

    // MessageType: 0x0011 (SkillCastResponse)
    message.push_back(0x11);
    message.push_back(0x00);

    // BodySize: 4 bytes
    uint32_t body_size = response.size();
    message.push_back((body_size >> 0) & 0xFF);
    message.push_back((body_size >> 8) & 0xFF);
    message.push_back((body_size >> 16) & 0xFF);
    message.push_back((body_size >> 24) & 0xFF);

    // Sequence: 4 bytes (使用0，因为不需要序列号)
    message.push_back(0);
    message.push_back(0);
    message.push_back(0);
    message.push_back(0);

    // Reserved: 2 bytes
    message.push_back(0);
    message.push_back(0);

    // 添加消息体
    message.insert(message.end(), response.begin(), response.end());

    // 发送响应
    auto response_buffer = std::make_shared<Core::Network::ByteBuffer>(message.data(), message.size());
    conn->AsyncSend(response_buffer, [result_code, skill_id, damage](const boost::system::error_code& ec, size_t) {
        if (ec) {
            spdlog::error("Failed to send SkillCastResponse: {}", ec.message());
        } else {
            spdlog::info("SkillCastResponse sent: result_code={}, skill_id={}, damage={}",
                        result_code, skill_id, damage);
        }
    });
}

// ========== 状态同步 ==========

void MapServerManager::BroadcastCharacterMovement(
    uint64_t character_id,
    float x, float y, float z,
    uint16_t direction,
    Core::Network::Connection::Ptr exclude_conn) {

    // 构造移动广播消息
    // 格式: [CharacterID:8][PosX:4][PosY:4][PosZ:4][Direction:2]

    std::vector<uint8_t> message;

    // CharacterID: 8 bytes
    for (int i = 0; i < 8; i++) {
        message.push_back((character_id >> (i * 8)) & 0xFF);
    }

    // Position X: 4 bytes
    uint32_t pos_x_bits;
    std::memcpy(&pos_x_bits, &x, 4);
    for (int i = 0; i < 4; i++) {
        message.push_back((pos_x_bits >> (i * 8)) & 0xFF);
    }

    // Position Y: 4 bytes
    uint32_t pos_y_bits;
    std::memcpy(&pos_y_bits, &y, 4);
    for (int i = 0; i < 4; i++) {
        message.push_back((pos_y_bits >> (i * 8)) & 0xFF);
    }

    // Position Z: 4 bytes
    uint32_t pos_z_bits;
    std::memcpy(&pos_z_bits, &z, 4);
    for (int i = 0; i < 4; i++) {
        message.push_back((pos_z_bits >> (i * 8)) & 0xFF);
    }

    // Direction: 2 bytes
    message.push_back((direction >> 0) & 0xFF);
    message.push_back((direction >> 8) & 0xFF);

    // 添加消息头: [MessageType:2][BodySize:4][Sequence:4][Reserved:2]
    std::vector<uint8_t> packet;

    // MessageType: 0x0012 (CharacterMovementBroadcast)
    packet.push_back(0x12);
    packet.push_back(0x00);

    // BodySize: 4 bytes
    uint32_t body_size = message.size();
    for (int i = 0; i < 4; i++) {
        packet.push_back((body_size >> (i * 8)) & 0xFF);
    }

    // Sequence: 4 bytes (使用0)
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    // Reserved: 2 bytes
    packet.push_back(0);
    packet.push_back(0);

    // 添加消息体
    packet.insert(packet.end(), message.begin(), message.end());

    // 查询附近的玩家（以新位置为中心，半径100单位）
    Murim::Position center(x, y, z);
    float broadcast_radius = 100.0f;  // 广播半径

    auto nearby_players = entity_manager_.QueryNearbyEntities(
        center, broadcast_radius, EntityType::kPlayer);

    spdlog::info("Broadcasting movement of character {} to {} nearby players",
                 character_id, nearby_players.size());

    // 向附近玩家广播（排除自己）
    int broadcast_count = 0;
    for (GameEntity* entity : nearby_players) {
        if (entity->entity_id == character_id) {
            continue;  // 跳过自己
        }

        // 从character_connections_查找对应的Connection
        auto it = character_connections_.find(entity->entity_id);
        if (it != character_connections_.end()) {
            auto target_conn = it->second;

            // 发送广播消息
            auto broadcast_buffer = std::make_shared<Core::Network::ByteBuffer>(
                packet.data(), packet.size());

            target_conn->AsyncSend(broadcast_buffer,
                [entity_id = entity->entity_id, character_id](
                    const boost::system::error_code& ec, size_t) {
                    if (!ec) {
                        spdlog::info("Movement broadcast sent: from={} to={}",
                                    character_id, entity_id);
                    } else {
                        spdlog::error("Failed to send movement broadcast to {}: {}",
                                    entity_id, ec.message());
                    }
                });

            broadcast_count++;
        } else {
            spdlog::warn("Connection not found for player {}", entity->entity_id);
        }
    }

    if (broadcast_count > 0) {
        spdlog::info("Movement broadcast sent to {} players", broadcast_count);
    }
}

void MapServerManager::BroadcastCharacterStats(
    uint64_t character_id,
    uint32_t hp, uint32_t max_hp,
    uint32_t mp, uint32_t max_mp,
    Core::Network::Connection::Ptr exclude_conn) {

    // 构造属性变化广播消息
    // 格式: [CharacterID:8][HP:4][MaxHP:4][MP:4][MaxMP:4]

    std::vector<uint8_t> message;

    // CharacterID: 8 bytes
    for (int i = 0; i < 8; i++) {
        message.push_back((character_id >> (i * 8)) & 0xFF);
    }

    // HP: 4 bytes
    for (int i = 0; i < 4; i++) {
        message.push_back((hp >> (i * 8)) & 0xFF);
    }

    // MaxHP: 4 bytes
    for (int i = 0; i < 4; i++) {
        message.push_back((max_hp >> (i * 8)) & 0xFF);
    }

    // MP: 4 bytes
    for (int i = 0; i < 4; i++) {
        message.push_back((mp >> (i * 8)) & 0xFF);
    }

    // MaxMP: 4 bytes
    for (int i = 0; i < 4; i++) {
        message.push_back((max_mp >> (i * 8)) & 0xFF);
    }

    // 添加消息头
    std::vector<uint8_t> packet;

    // MessageType: 0x0013 (CharacterStatsBroadcast)
    packet.push_back(0x13);
    packet.push_back(0x00);

    // BodySize: 4 bytes
    uint32_t body_size = message.size();
    for (int i = 0; i < 4; i++) {
        packet.push_back((body_size >> (i * 8)) & 0xFF);
    }

    // Sequence: 4 bytes
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    // Reserved: 2 bytes
    packet.push_back(0);
    packet.push_back(0);

    // 添加消息体
    packet.insert(packet.end(), message.begin(), message.end());

    // 获取角色位置
    auto character_opt = character_manager_.GetCharacter(character_id);
    if (!character_opt.has_value()) {
        spdlog::error("Character not found for stats broadcast: {}", character_id);
        return;
    }

    const auto& character = character_opt.value();
    Murim::Position center(character.x, character.y, character.z);
    float broadcast_radius = 100.0f;

    // 查询附近玩家
    auto nearby_players = entity_manager_.QueryNearbyEntities(
        center, broadcast_radius, EntityType::kPlayer);

    spdlog::info("Broadcasting stats of character {} (HP {}/{}, MP {}/{}) to {} nearby players",
                 character_id, hp, max_hp, mp, max_mp, nearby_players.size());

    // 向附近玩家广播
    int broadcast_count = 0;
    for (GameEntity* entity : nearby_players) {
        if (entity->entity_id == character_id) {
            continue;  // 跳过自己
        }

        // 从character_connections_查找对应的Connection
        auto it = character_connections_.find(entity->entity_id);
        if (it != character_connections_.end()) {
            auto target_conn = it->second;

            // 发送广播消息
            auto broadcast_buffer = std::make_shared<Core::Network::ByteBuffer>(
                packet.data(), packet.size());

            target_conn->AsyncSend(broadcast_buffer,
                [entity_id = entity->entity_id, character_id, hp, max_hp, mp, max_mp](
                    const boost::system::error_code& ec, size_t) {
                    if (!ec) {
                        spdlog::info("Stats broadcast sent: from={} to={}, HP {}/{}, MP {}/{}",
                                    character_id, entity_id, hp, max_hp, mp, max_mp);
                    } else {
                        spdlog::error("Failed to send stats broadcast to {}: {}",
                                    entity_id, ec.message());
                    }
                });

            broadcast_count++;
        } else {
            spdlog::warn("Connection not found for player {}", entity->entity_id);
        }
    }

    if (broadcast_count > 0) {
        spdlog::info("Stats broadcast sent to {} players", broadcast_count);
    }
}

void MapServerManager::AddPlayer(uint64_t character_id) {
    // 玩家已通过认证,添加到服务器
    spdlog::info("Player added to server: character_id={}", character_id);
}

void MapServerManager::RemovePlayer(uint64_t character_id) {
    // 移除玩家
    if (players_.find(character_id) != players_.end()) {
        players_.erase(character_id);
    }
}

bool MapServerManager::IsPlayerOnline(uint64_t character_id) const {
    return players_.find(character_id) != players_.end();
}

// ========== 服务器循环 ==========

void MapServerManager::MainLoop() {
    spdlog::info("MapServer main loop started");

    const std::chrono::milliseconds tick_interval(tick_rate_);

    while (status_ == ServerStatus::kRunning) {
        auto start = std::chrono::steady_clock::now();

        // 处理服务器tick
        ProcessTick();

        // 计算处理时间
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // 如果处理时间小于tick间隔,则sleep剩余时间
        if (elapsed < tick_interval) {
            std::this_thread::sleep_for(tick_interval - elapsed);
        }
    }

    spdlog::info("MapServer main loop stopped");
}

void MapServerManager::Update(uint32_t delta_time) {
    // 对应 legacy CServerSystem::Process() (ServerSystem.cpp:737)

    // 1. 时间管理器处理 - 对应 legacy: MHTIMEMGR_OBJ->Process() (ServerSystem.cpp:749)
    // 时间系统自动更新,通过delta_time参数传递

    // 2. AI系统更新 - 对应 legacy: AISystem处理
    // 在legacy中,AI系统通过对象处理时调用,这里统一调用
    // AIManager::Process() 需要当前服务器时间(毫秒),而不是delta_time
    auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    ai_manager_.Process(static_cast<uint32_t>(current_time));

    // 3. 技能系统更新 - 对应 legacy: 技能冷却和状态更新
    // 技能管理器需要处理技能冷却时间、持续效果等
    skill_manager_.Update(delta_time);

    // 4. 任务系统更新 - 对应 legacy: QUESTMGR->Process() (ServerSystem.cpp:813)
    // 任务管理器处理任务时间检查、事件处理等
    quest_manager_.Process();

    // 5. 队伍系统更新 - 对应 legacy: PARTYMGR->Process() (ServerSystem.cpp:892)
    // 队伍管理器处理队伍经验分配、成员状态等
    party_manager_.Process();

    // 6. 聊天系统更新 - 对应 legacy: 聊天消息处理
    // 聊天管理器处理消息队列、过滤、广播等
    chat_manager_.Process();

    // 7. 刷新系统更新 - 对应 legacy: RegenManager处理
    // 对应 legacy: AIGroupManager->RegenProcess() (ServerSystem.cpp中多处)
    // 对应 legacy: FieldBossMonsterManager->Process() (ServerSystem.cpp中调用)
    // 刷怪点管理器处理怪物刷新、复活等
    spawn_point_manager_.Update(delta_time);

    // 8. 事务处理系统 - 对应 legacy: 各种Manager的Process调用
    // 包括: PartyManager, GuildManager, QuestManager等已在上面处理

    // 9. 物品系统更新 - 对应 legacy: 物品掉落、过期检查等
    // 物品管理器处理物品掉落、装备耐久、过期物品等
    item_manager_.Update(delta_time);

    // 10. 角色系统更新 - 对应 legacy: 角色状态、生命恢复等
    // 角色管理器处理角色生命值、内力恢复等
    character_manager_.Update(delta_time);

    // 对应 legacy: GridSystem->GridProcess() (ServerSystem.cpp:775)
    // 格子系统处理将在对象处理时自动进行

    // 对应 legacy: SiegeWar, Weather, Event等特殊系统
    // 这些系统将在后续扩展时添加

    // 暂时禁用此日志,避免刷屏
    // spdlog::debug("MapServer updated: delta_time={}ms, players={}", delta_time, players_.size());
}

// ========== 辅助方法 ==========

void MapServerManager::ProcessTick() {
    uint32_t delta_time = tick_rate_;

    // 更新服务器状态
    Update(delta_time);

    // 广播服务器状态 (暂时禁用,避免日志刷屏)
    // BroadcastServerStatus();
}

void MapServerManager::BroadcastServerStatus() {
    // 对应 legacy: 广播服务器状态给所有玩家
    // 暂时禁用此日志,避免刷屏
    // spdlog::debug("Broadcast server status to {} players", players_.size());
}

// ========== 死亡处理 ==========

void MapServerManager::HandleCharacterDeath(uint64_t character_id, uint64_t killer_id, Core::Network::Connection::Ptr conn) {
    spdlog::info("Character died: character_id={}, killer_id={}", character_id, killer_id);

    // 获取角色信息
    auto character_opt = character_manager_.GetCharacter(character_id);
    if (!character_opt.has_value()) {
        spdlog::error("Character not found for death handling: {}", character_id);
        return;
    }

    auto character = character_opt.value();

    // 更新角色状态为"已死亡"
    character.status = Game::CharacterStatus::kDead;
    character.death_time = std::chrono::system_clock::now();

    spdlog::info("Character status updated: character_id={}, status=DEAD, death_time={}",
                 character_id, std::chrono::system_clock::to_time_t(character.death_time));

    // 保存状态到数据库
    // 注意: 需要数据库表有 status 和 death_time 字段
    // 当前使用内存中的CharacterManager更新，数据库持久化在SaveCharacter中实现
    bool save_success = character_manager_.SaveCharacter(character);
    if (!save_success) {
        spdlog::warn("Failed to save character death status to database: character_id={}", character_id);
    } else {
        spdlog::info("Character death status saved to database: character_id={}", character_id);
    }

    // 经验惩罚（死亡扣除5%经验）
    const double DEATH_EXP_PENALTY_RATIO = 0.05;  // 5%经验惩罚
    uint64_t exp_penalty = static_cast<uint64_t>(character.exp * DEATH_EXP_PENALTY_RATIO);

    // 确保至少扣除1点经验（如果经验大于0）
    if (character.exp > 0 && exp_penalty < 1) {
        exp_penalty = 1;
    }

    // 计算扣除后的经验（防止降级）
    uint64_t new_exp = character.exp - exp_penalty;
    const uint64_t LEVEL_1_EXP = 0;  // 1级所需经验为0

    // 防止降级：确保经验不低于当前等级的最低经验
    if (new_exp < LEVEL_1_EXP) {
        new_exp = LEVEL_1_EXP;
        exp_penalty = character.exp - new_exp;  // 调整惩罚值为实际扣除值
    }

    // 应用经验惩罚
    if (exp_penalty > 0) {
        character.exp = new_exp;

        spdlog::info("Death penalty applied: character_id={}, exp_penalty={}, old_exp={}, new_exp={}",
                     character_id, exp_penalty, character.exp + exp_penalty, new_exp);

        // 保存经验变化到数据库
        character_manager_.SaveCharacter(character);

        // TODO: 发送经验变化消息给客户端
        // SendExpChange(character_id, -static_cast<int64_t>(exp_penalty));
    } else {
        spdlog::info("No death penalty applied: character_id={}, exp=0", character_id);
    }

    // 清理角色的临时状态 - 清除所有 Buff 和 Debuff
    size_t buffs_removed = Game::BuffManager::Instance().RemoveAllBuffs(character_id, true);
    spdlog::info("Cleared buffs on death: character_id={}, buffs_cleared={}", character_id, buffs_removed);

    // TODO: 清空仇恨列表（AI系统）
    // AIManager::Instance().ClearHate(character_id);

    // 生成掉落物品
    // 简化版：根据角色等级生成金币掉落
    uint32_t gold_drop = character.level * 10;  // 每级10金币

    // 添加随机波动（±20%）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(-20, 20);
    int variation = (gold_drop * dis(gen)) / 100;
    gold_drop = std::max(1u, gold_drop + variation);  // 至少1金币

    // 创建掉落物品列表
    std::vector<Game::LootItem> loot_items;
    loot_items.emplace_back(10001, gold_drop, 1, killer_id);  // item_id=10001(金币), 数量, 品质, 所有者

    // 生成掉落物
    uint64_t loot_id = Game::LootManager::Instance().CreateLootDrop(
        character_id,              // source_id
        character.x, character.y, character.z,  // 位置
        character.map_id,          // 地图ID
        loot_items,                // 物品列表
        killer_id,                 // 所有者（击杀者优先拾取）
        30,                        // 30秒保护时间
        300                        // 5分钟存在时间
    );

    if (loot_id > 0) {
        spdlog::info("Loot generated: loot_id={}, gold={}, owner_id={}", loot_id, gold_drop, killer_id);

        // TODO: 广播掉落生成消息给附近玩家
        // SendLootSpawn(loot_id, character.x, character.y, character.z, loot_items);
    }

    // 从实体管理器中移除死亡角色（可选，保留尸体可见）
    // entity_manager_.RemoveEntity(character_id);

    spdlog::info("Character death handled: character_id={}, killer_id={}", character_id, killer_id);

    // 广播死亡给附近玩家
    BroadcastCharacterDeath(character_id, killer_id, conn);

    // TODO: 发送复活倒计时
    // SendReviveTimer(character_id, revive_time);
}

void MapServerManager::BroadcastCharacterDeath(
    uint64_t character_id,
    uint64_t killer_id,
    Core::Network::Connection::Ptr exclude_conn) {

    spdlog::info("Broadcasting death of character {} (killer={}) to nearby players",
                 character_id, killer_id);

    // 构造死亡广播消息
    // 格式: [Header:12bytes][Body:16bytes]
    // Body: [CharacterID:8][KillerID:8]

    std::vector<uint8_t> message;

    // CharacterID: 8 bytes (little-endian)
    for (int i = 0; i < 8; i++) {
        message.push_back((character_id >> (i * 8)) & 0xFF);
    }

    // KillerID: 8 bytes (little-endian)
    for (int i = 0; i < 8; i++) {
        message.push_back((killer_id >> (i * 8)) & 0xFF);
    }

    // 添加消息头
    std::vector<uint8_t> packet;

    // MessageType: 0x0014 (CharacterDeathBroadcast)
    packet.push_back(0x14);
    packet.push_back(0x00);

    // BodySize: 4 bytes
    uint32_t body_size = message.size();
    for (int i = 0; i < 4; i++) {
        packet.push_back((body_size >> (i * 8)) & 0xFF);
    }

    // Sequence: 4 bytes
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    // Reserved: 2 bytes
    packet.push_back(0);
    packet.push_back(0);

    // 添加消息体
    packet.insert(packet.end(), message.begin(), message.end());

    // 获取角色位置
    auto character_opt = character_manager_.GetCharacter(character_id);
    if (!character_opt.has_value()) {
        spdlog::error("Character not found for death broadcast: {}", character_id);
        return;
    }

    const auto& character = character_opt.value();
    Murim::Position center(character.x, character.y, character.z);
    float broadcast_radius = 100.0f;

    // 查询附近玩家
    auto nearby_players = entity_manager_.QueryNearbyEntities(
        center, broadcast_radius, EntityType::kPlayer);

    spdlog::info("Broadcasting death to {} nearby players", nearby_players.size());

    // 向附近玩家广播死亡消息
    int broadcast_count = 0;
    for (GameEntity* entity : nearby_players) {
        // 从character_connections_查找对应的Connection
        auto it = character_connections_.find(entity->entity_id);
        if (it != character_connections_.end()) {
            auto target_conn = it->second;

            // 发送死亡广播消息
            auto broadcast_buffer = std::make_shared<Core::Network::ByteBuffer>(
                packet.data(), packet.size());

            target_conn->AsyncSend(broadcast_buffer, [character_id, killer_id](
                const boost::system::error_code& ec, size_t) {
                if (ec) {
                    spdlog::error("Failed to send death broadcast: char_id={}, killer_id={}, error={}",
                                character_id, killer_id, ec.message());
                } else {
                    spdlog::debug("Death broadcast sent: char_id={}, killer_id={}",
                                character_id, killer_id);
                }
            });

            broadcast_count++;
        }
    }

    spdlog::info("Death broadcast sent to {} players", broadcast_count);
}

// ========== 复活处理 ==========

bool MapServerManager::HandleCharacterRevive(uint64_t character_id, uint8_t revive_type, Core::Network::Connection::Ptr conn) {
    spdlog::info("Character revive request: character_id={}, revive_type={}", character_id, revive_type);

    // 获取角色信息
    auto character_opt = character_manager_.GetCharacter(character_id);
    if (!character_opt.has_value()) {
        spdlog::error("Character not found for revive: {}", character_id);
        SendReviveResponse(conn, 1, character_id, 0, 0, 0);  // Error: character not found
        return false;
    }

    auto character = character_opt.value();

    // 检查角色是否已死亡
    if (character.IsAlive()) {
        spdlog::warn("Character is not dead, cannot revive: character_id={}", character_id);
        SendReviveResponse(conn, 3, character_id, character.x, character.y, character.z);  // Error: not dead
        return false;
    }

    // 检查复活冷却
    const uint32_t REVIVE_COOLDOWN_SECONDS = 60;  // 60秒冷却时间

    if (character.IsReviveCoolingDown(REVIVE_COOLDOWN_SECONDS)) {
        uint32_t remaining = character.GetReviveCooldownRemaining(REVIVE_COOLDOWN_SECONDS);
        spdlog::warn("Character revive on cooldown: character_id={}, remaining={}s",
                     character_id, remaining);
        // TODO: 修改 SendReviveResponse 支持传递冷却剩余时间
        // 暂时使用错误码 2 表示冷却中
        SendReviveResponse(conn, 2, character_id, 0, 0, 0);  // Error: cooldown
        return false;
    }

    // 确定复活位置
    float revive_x, revive_y, revive_z;

    if (revive_type == 0) {
        // 城镇复活: 传送到城镇安全区
        revive_x = 1250.0f;
        revive_y = 1000.0f;
        revive_z = 0.0f;
        spdlog::info("Town revive: character_id={}, location=({:.1f}, {:.1f}, {:.1f})",
                     character_id, revive_x, revive_y, revive_z);
    } else if (revive_type == 1) {
        // 原地复活: 在死亡位置复活
        revive_x = character.x;
        revive_y = character.y;
        revive_z = character.z;
        spdlog::info("In-place revive: character_id={}, location=({:.1f}, {:.1f}, {:.1f})",
                     character_id, revive_x, revive_y, revive_z);
    } else {
        spdlog::warn("Unknown revive type: {}", revive_type);
        SendReviveResponse(conn, 1, character_id, 0, 0, 0);  // Error: invalid revive type
        return false;
    }

    // 恢复角色状态
    character.status = Game::CharacterStatus::kAlive;

    // 恢复HP和MP（恢复一定比例，例如30%）
    character.hp = character.max_hp * 3 / 10;  // 恢复30% HP
    character.mp = character.max_mp * 3 / 10;  // 恢复30% MP

    // 更新位置
    character.x = revive_x;
    character.y = revive_y;
    character.z = revive_z;

    // 更新最后复活时间（用于冷却）
    character.last_revive_time = std::chrono::system_clock::now();

    // 设置复活保护时间（复活后无敌）
    character.revive_protection_time = std::chrono::system_clock::now();

    spdlog::info("Character revived: character_id={}, HP={}/{}, MP={}/{}, status=ALIVE, protection=3s",
                 character_id, character.hp, character.max_hp, character.mp, character.max_mp);

    // 保存到数据库
    bool save_success = character_manager_.SaveCharacter(character);
    if (!save_success) {
        spdlog::warn("Failed to save revived character to database: character_id={}", character_id);
    } else {
        spdlog::info("Revived character saved to database: character_id={}", character_id);
    }

    // 发送复活响应
    SendReviveResponse(conn, 0, character_id, revive_x, revive_y, revive_z);

    // 广播角色状态变化（HP/MP）
    BroadcastCharacterStats(character_id, character.hp, character.max_hp,
                           character.mp, character.max_mp, conn);

    // 广播角色移动（复活位置）
    // 注意: 这里可能需要创建一个特殊的"复活移动"广播消息
    // 暂时使用现有的移动广播

    spdlog::info("Character revive completed: character_id={}", character_id);

    return true;
}

void MapServerManager::SendReviveResponse(
    Core::Network::Connection::Ptr conn,
    int32_t result_code,
    uint64_t character_id,
    float revive_x,
    float revive_y,
    float revive_z
) {
    // 构造复活响应消息
    // 格式: [Header:12bytes][Body:24bytes]
    // Body: [ResultCode:4][CharacterID:8][ReviveX:4][ReviveY:4][ReviveZ:4]

    std::vector<uint8_t> packet;
    packet.reserve(12 + 24);

    // 消息头
    uint16_t message_type = 0x0015;  // ReviveResponse
    uint32_t body_size = 24;
    uint32_t sequence = 0;
    uint16_t reserved = 0;

    // MessageType: 2 bytes
    packet.push_back((message_type >> 0) & 0xFF);
    packet.push_back((message_type >> 8) & 0xFF);

    // BodySize: 4 bytes
    packet.push_back((body_size >> 0) & 0xFF);
    packet.push_back((body_size >> 8) & 0xFF);
    packet.push_back((body_size >> 16) & 0xFF);
    packet.push_back((body_size >> 24) & 0xFF);

    // Sequence: 4 bytes
    packet.push_back((sequence >> 0) & 0xFF);
    packet.push_back((sequence >> 8) & 0xFF);
    packet.push_back((sequence >> 16) & 0xFF);
    packet.push_back((sequence >> 24) & 0xFF);

    // Reserved: 2 bytes
    packet.push_back(0);
    packet.push_back(0);

    // 消息体
    // ResultCode: 4 bytes (0=成功, 1=失败, 2=冷却中, 3=未死亡)
    for (int i = 0; i < 4; i++) {
        packet.push_back((result_code >> (i * 8)) & 0xFF);
    }

    // CharacterID: 8 bytes
    for (int i = 0; i < 8; i++) {
        packet.push_back((character_id >> (i * 8)) & 0xFF);
    }

    // ReviveX: 4 bytes (float)
    uint32_t x_bits;
    std::memcpy(&x_bits, &revive_x, 4);
    for (int i = 0; i < 4; i++) {
        packet.push_back((x_bits >> (i * 8)) & 0xFF);
    }

    // ReviveY: 4 bytes (float)
    uint32_t y_bits;
    std::memcpy(&y_bits, &revive_y, 4);
    for (int i = 0; i < 4; i++) {
        packet.push_back((y_bits >> (i * 8)) & 0xFF);
    }

    // ReviveZ: 4 bytes (float)
    uint32_t z_bits;
    std::memcpy(&z_bits, &revive_z, 4);
    for (int i = 0; i < 4; i++) {
        packet.push_back((z_bits >> (i * 8)) & 0xFF);
    }

    // 发送响应
    auto buffer = std::make_shared<Core::Network::ByteBuffer>(packet.data(), packet.size());
    conn->AsyncSend(buffer, [character_id, result_code](
        const boost::system::error_code& ec, size_t) {
        if (ec) {
            spdlog::warn("Failed to send revive response: character_id={}, error={}",
                         character_id, ec.message());
        } else {
            spdlog::debug("Revive response sent: character_id={}, result_code={}",
                          character_id, result_code);
        }
    });
}

// ========== 拾取系统 ==========

bool MapServerManager::HandleLootRequest(uint64_t character_id, uint64_t loot_id, Core::Network::Connection::Ptr conn) {
    spdlog::info("Loot request: character_id={}, loot_id={}", character_id, loot_id);

    // 获取掉落物
    auto loot_opt = Game::LootManager::Instance().GetLootDrop(loot_id);
    if (!loot_opt.has_value()) {
        spdlog::warn("Loot not found: loot_id={}", loot_id);
        SendLootResponse(conn, 2, loot_id);  // Error: not found
        return false;
    }

    const auto& loot = loot_opt.value();

    // 检查拾取保护
    if (!loot.CanLoot(character_id)) {
        spdlog::warn("Loot protected: loot_id={}, owner_id={}, character_id={}",
                     loot_id, loot.owner_id, character_id);
        SendLootResponse(conn, 3, loot_id);  // Error: no permission
        return false;
    }

    // 检查背包空间
    uint16_t empty_slots = item_manager_.GetEmptySlotCount(character_id);
    if (empty_slots < loot.items.size()) {
        spdlog::warn("Inventory full: character_id={}, empty_slots={}, required={}",
                     character_id, empty_slots, loot.items.size());
        SendLootResponse(conn, 4, loot_id);  // Error: inventory full
        return false;
    }

    // 拾取物品到背包
    bool all_items_added = true;
    for (const auto& loot_item : loot.items) {
        uint64_t instance_id = item_manager_.ObtainItem(
            character_id,
            loot_item.item_id,
            loot_item.quantity
        );

        if (instance_id == 0) {
            spdlog::error("Failed to add item to inventory: character_id={}, item_id={}, quantity={}",
                         character_id, loot_item.item_id, loot_item.quantity);
            all_items_added = false;
        } else {
            spdlog::info("Item looted successfully: character_id={}, item_id={}, quantity={}, instance_id={}",
                         character_id, loot_item.item_id, loot_item.quantity, instance_id);
        }
    }

    // 如果有任何物品添加失败，返回错误
    if (!all_items_added) {
        SendLootResponse(conn, 1, loot_id);  // Error: failed
        return false;
    }

    // 移除掉落物
    Game::LootManager::Instance().RemoveLootDrop(loot_id);

    // 发送成功响应
    SendLootResponse(conn, 0, loot_id);  // Success

    spdlog::info("Loot completed: character_id={}, loot_id={}", character_id, loot_id);

    return true;
}

void MapServerManager::SendLootResponse(
    Core::Network::Connection::Ptr conn,
    int32_t result_code,
    uint64_t loot_id
) {
    // 构造拾取响应消息
    // 格式: [Header:12bytes][Body:12bytes]
    // Body: [ResultCode:4][LootID:8]

    std::vector<uint8_t> packet;
    packet.reserve(12 + 12);

    // 消息头
    uint16_t message_type = 0x0016;  // LootResponse
    uint32_t body_size = 12;
    uint32_t sequence = 0;
    uint16_t reserved = 0;

    // MessageType: 2 bytes
    packet.push_back((message_type >> 0) & 0xFF);
    packet.push_back((message_type >> 8) & 0xFF);

    // BodySize: 4 bytes
    packet.push_back((body_size >> 0) & 0xFF);
    packet.push_back((body_size >> 8) & 0xFF);
    packet.push_back((body_size >> 16) & 0xFF);
    packet.push_back((body_size >> 24) & 0xFF);

    // Sequence: 4 bytes
    packet.push_back((sequence >> 0) & 0xFF);
    packet.push_back((sequence >> 8) & 0xFF);
    packet.push_back((sequence >> 16) & 0xFF);
    packet.push_back((sequence >> 24) & 0xFF);

    // Reserved: 2 bytes
    packet.push_back(0);
    packet.push_back(0);

    // 消息体
    // ResultCode: 4 bytes (0=成功, 1=失败, 2=不存在, 3=无权拾取, 4=背包满)
    for (int i = 0; i < 4; i++) {
        packet.push_back((result_code >> (i * 8)) & 0xFF);
    }

    // LootID: 8 bytes
    for (int i = 0; i < 8; i++) {
        packet.push_back((loot_id >> (i * 8)) & 0xFF);
    }

    // 发送响应
    auto buffer = std::make_shared<Core::Network::ByteBuffer>(packet.data(), packet.size());
    conn->AsyncSend(buffer, [loot_id, result_code](
        const boost::system::error_code& ec, size_t) {
        if (ec) {
            spdlog::warn("Failed to send loot response: loot_id={}, error={}",
                         loot_id, ec.message());
        } else {
            spdlog::debug("Loot response sent: loot_id={}, result_code={}",
                          loot_id, result_code);
        }
    });
}

// ========== 怪物系统实现 ==========

const Game::MonsterTemplate* MapServerManager::GetMonsterTemplate(uint32_t monster_id) const {
    auto it = monster_templates_.find(monster_id);
    if (it != monster_templates_.end()) {
        return &(it->second);
    }
    return nullptr;
}

bool MapServerManager::HasMonsterTemplate(uint32_t monster_id) const {
    return monster_templates_.find(monster_id) != monster_templates_.end();
}

bool MapServerManager::LoadMonsterSystem() {
    using namespace Murim::Core::Database;

    spdlog::info("Loading monster templates from database...");

    try {
        // 从数据库加载所有怪物模板
        monster_templates_ = MonsterDAO::LoadAllMonsterTemplates();

        if (monster_templates_.empty()) {
            spdlog::warn("No monster templates loaded from database, using fallback test data");

            // Fallback: 添加测试怪物模板
            Game::MonsterTemplate tmpl;
            tmpl.monster_id = 1001;
            tmpl.monster_name = "测试怪物";
            tmpl.level = 10;
            tmpl.hp = 1000;
            tmpl.attack_power_min = 50;
            tmpl.attack_power_max = 80;
            tmpl.run_speed = 150;
            tmpl.search_range = 500;
            tmpl.domain_range = 1000;
            monster_templates_[tmpl.monster_id] = tmpl;
        }

        spdlog::info("Loaded {} monster templates from database", monster_templates_.size());

        // 加载刷怪点配置
        if (!spawn_point_manager_.LoadSpawnPoints()) {
            spdlog::warn("Failed to load spawn points from database, using test data");
        }

        spdlog::info("Loaded {} spawn points", spawn_point_manager_.GetTotalSpawnPoints());

        // 记录一些示例怪物
        spdlog::info("Sample monsters:");
        size_t count = 0;
        for (const auto& [monster_id, monster_template] : monster_templates_) {
            if (count >= 5) break;

            spdlog::info("  [{}] {} (Lv.{}, HP:{}, ATK:{}-{}, 索敌:{}, 活动:{})",
                        monster_template.monster_id, monster_template.monster_name,
                        monster_template.level, monster_template.hp,
                        monster_template.attack_power_min, monster_template.attack_power_max,
                        monster_template.search_range,
                        monster_template.domain_range);
            count++;
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load monster system: {}", e.what());
        return false;
    }
}

} // namespace MapServer
} // namespace Murim
