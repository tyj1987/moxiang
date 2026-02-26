#pragma once

#include <memory>
#include <unordered_map>
#include <cstdint>
#include <boost/asio.hpp>
#include "shared/character/CharacterManager.hpp"
#include "shared/character/MonsterTemplate.hpp"
#include "shared/skill/SkillManager.hpp"
#include "shared/item/ItemManager.hpp"
#include "shared/quest/Quest.hpp"
#include "shared/ai/AI.hpp"
#include "shared/social/Friend.hpp"
#include "shared/social/Party.hpp"
#include "shared/social/Chat.hpp"
#include "shared/movement/MovementValidator.hpp"
#include "shared/movement/PositionBroadcaster.hpp"
#include "shared/item/ItemHandler.hpp"
#include "shared/battle/DamageCalculator.hpp"
#include "shared/battle/SpecialStateManager.hpp"
#include "EntityManager.hpp"
#include "ServerCommunicationManager.hpp"
#include "SpawnPointManager.hpp"
#include "shared/npc/NPCManager.hpp"
#include "shared/shop/ShopManager.hpp"
#include "shared/warehouse/WarehouseManager.hpp"
#include "shared/mail/MailManager.hpp"
#include "shared/market/MarketManager.hpp"
#include "shared/trade/TradeManager.hpp"
#include "shared/leaderboard/LeaderboardManager.hpp"
#include "shared/achievement/AchievementManager.hpp"
#include "shared/title/TitleManager.hpp"
// TODO: 暂时注释，API 不匹配需要重构
// #include "shared/pet/PetManager.hpp"
// #include "shared/mount/MountManager.hpp"
#include "shared/vip/VIPManager.hpp"
#include "shared/dungeon/DungeonManager.hpp"
// TODO: 需要重构 - 使用 pqxx 而不是 libpq
// #include "shared/signin/SignInManager.hpp"
// #include "shared/marriage/MarriageManager.hpp"
#include "DialogueManager.hpp"
#include "core/network/IOContext.hpp"
#include "core/network/Acceptor.hpp"

namespace Murim {
namespace MapServer {

// Using statements for Manager types (inside namespace Murim)
using CharacterManager = Murim::Game::CharacterManager;
using SkillManager = Murim::Game::SkillManager;
using ItemManager = Murim::Game::ItemManager;
using QuestManager = Murim::Game::QuestManager;
using AIManager = Murim::Game::AIManager;
using FriendManager = Murim::Game::FriendManager;
using PartyManager = Murim::Game::PartyManager;
using ChatManager = Murim::Game::ChatManager;
using MonsterTemplate = Murim::Game::MonsterTemplate;
using NPCManager = Murim::Game::NPCManager;
using ShopManager = Murim::Game::ShopManager;
using WarehouseManager = Murim::Game::WarehouseManager;
using MailManager = Murim::Game::MailManager;
using MarketManager = Murim::Game::MarketManager;
using TradeManager = Murim::Game::TradeManager;
using LeaderboardManager = Murim::Game::LeaderboardManager;
using AchievementManager = Murim::Game::AchievementManager;
using TitleManager = Murim::Game::TitleManager;
// TODO: 暂时注释，API 不匹配需要重构
// using PetManager = Murim::Game::PetManager;
// using MountManager = Murim::Game::MountManager;
using VIPManager = Murim::Game::VIPManager;
// TODO: 暂时注释，需要 libpq 支持
// using SignInManager = Murim::Game::SignInManager;
// using MarriageManager = Murim::Game::MarriageManager;

/**
 * @brief MapServer管理器
 *
 * 负责MapServer的核心逻辑,集成所有Manager
 * 对应 legacy: CServer / Server类
 */
class MapServerManager {
public:
    /**
     * @brief 获取单例实例
     */
    static MapServerManager& Instance();

    // ========== 初始化 ==========

    /**
     * @brief 初始化MapServer
     * @return 是否初始化成功
     */
    bool Initialize();

    /**
     * @brief 启动MapServer
     * @return 是否启动成功
     */
    bool Start();

    /**
     * @brief 停止MapServer
     */
    void Stop();

    /**
     * @brief 关闭MapServer
     */
    void Shutdown();

    // ========== Manager访问 ==========

    /**
     * @brief 获取角色管理器
     */
    CharacterManager& GetCharacterManager() { return character_manager_; }

    /**
     * @brief 获取技能管理器
     */
    SkillManager& GetSkillManager() { return skill_manager_; }

    /**
     * @brief 获取物品管理器
     */
    ItemManager& GetItemManager() { return item_manager_; }

    /**
     * @brief 获取任务管理器
     */
    QuestManager& GetQuestManager() { return quest_manager_; }

    /**
     * @brief 获取AI管理器
     */
    AIManager& GetAIManager() { return ai_manager_; }

    /**
     * @brief 获取好友管理器
     */
    FriendManager& GetFriendManager() { return friend_manager_; }

    /**
     * @brief 获取队伍管理器
     */
    PartyManager& GetPartyManager() { return party_manager_; }

    /**
     * @brief 获取聊天管理器
     */
    ChatManager& GetChatManager() { return chat_manager_; }

    /**
     * @brief 获取实体管理器
     */
    EntityManager& GetEntityManager() { return entity_manager_; }

    /**
     * @brief 获取刷怪点管理器
     */
    SpawnPointManager& GetSpawnPointManager() { return spawn_point_manager_; }

    /**
     * @brief 获取服务器通信管理器
     */
    ServerCommunicationManager& GetServerCommunicationManager() { return server_comm_manager_; }

    /**
     * @brief 获取副本管理器
     */
    Game::DungeonManager& GetDungeonManager() { return dungeon_manager_; }

    // TODO: 暂时注释，需要 libpq 支持
    // /**
    //  * @brief 获取签到管理器
    //  */
    // SignInManager& GetSignInManager() { return signin_manager_; }
    //
    // /**
    //  * @brief 获取结婚管理器
    //  */
    // MarriageManager& GetMarriageManager() { return marriage_manager_; }

    // ========== 怪物系统 ==========

    /**
     * @brief 获取怪物模板
     * @param monster_id 怪物ID
     * @return 怪物模板（如果存在）
     */
    const MonsterTemplate* GetMonsterTemplate(uint32_t monster_id) const;

    /**
     * @brief 检查怪物模板是否存在
     * @param monster_id 怪物ID
     * @return 是否存在
     */
    bool HasMonsterTemplate(uint32_t monster_id) const;

    /**
     * @brief 获取怪物模板数量
     * @return 怪物模板总数
     */
    size_t GetMonsterTemplateCount() const { return monster_templates_.size(); }

    // ========== 状态查询 ==========

    /**
     * @brief 获取服务器状态
     */
    enum class ServerStatus {
        kStopped,    // 已停止
        kStarting,   // 启动中
        kRunning,    // 运行中
        kStopping    // 停止中
    };

    ServerStatus GetStatus() const { return status_; }

    /**
     * @brief 获取服务器ID
     */
    uint16_t GetServerId() const { return server_id_; }

    /**
     * @brief 获取地图ID
     */
    uint16_t GetMapId() const { return map_id_; }

    /**
     * @brief 获取当前玩家数量
     */
    size_t GetPlayerCount() const { return players_.size(); }

    // ========== 玩家管理 ==========

    /**
     * @brief 玩家登录
     * @param character_id 角色ID
     * @param session_id 会话ID
     * @param conn 客户端连接
     * @return 是否成功
     */
    bool OnPlayerLogin(uint64_t character_id, uint64_t session_id, Core::Network::Connection::Ptr conn);

    /**
     * @brief 玩家登出
     * @param character_id 角色ID
     */
    void OnPlayerLogout(uint64_t character_id);

    // ========== 网络连接处理 ==========

    /**
     * @brief 处理客户端连接
     * @param conn 连接对象
     */
    void HandleClientConnection(Core::Network::Connection::Ptr conn);

    /**
     * @brief 处理客户端消息
     * @param conn 连接对象
     * @param buffer 消息缓冲区
     */
    void HandleClientMessage(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 添加在线玩家
     */
    void AddPlayer(uint64_t character_id);

    /**
     * @brief 移除在线玩家
     */
    void RemovePlayer(uint64_t character_id);

    /**
     * @brief 检查玩家是否在线
     */
    bool IsPlayerOnline(uint64_t character_id) const;

    // ========== 服务器循环 ==========

    /**
     * @brief 服务器主循环
     */
    void MainLoop();

    /**
     * @brief 更新服务器状态
     * @param delta_time 时间增量(毫秒)
     */
    void Update(uint32_t delta_time);

    private:
    MapServerManager();
    ~MapServerManager() = default;
    MapServerManager(const MapServerManager&) = delete;
    MapServerManager& operator=(const MapServerManager&) = delete;

    // ========== 服务器状态 ==========

    ServerStatus status_ = ServerStatus::kStopped;
    uint16_t server_id_ = 0;
    uint16_t map_id_ = 1;

    // ========== Manager实例 ==========

    CharacterManager& character_manager_;
    SkillManager& skill_manager_;
    ItemManager& item_manager_;
    QuestManager& quest_manager_;
    AIManager& ai_manager_;
    FriendManager& friend_manager_;
    PartyManager& party_manager_;
    ChatManager& chat_manager_;
    EntityManager& entity_manager_;
    SpawnPointManager spawn_point_manager_;
    ServerCommunicationManager& server_comm_manager_;

    // ========== NPC & 交互系统 ==========

    Game::NPCManager& npc_manager_;
    Game::ShopManager& shop_manager_;
    Game::WarehouseManager& warehouse_manager_;
    Game::MailManager& mail_manager_;
    Game::MarketManager& market_manager_;
    Game::TradeManager& trade_manager_;
    Game::LeaderboardManager& leaderboard_manager_;
    Game::AchievementManager& achievement_manager_;
    Game::TitleManager& title_manager_;
    // TODO: 暂时注释，API 不匹配需要重构
    // Game::PetManager& pet_manager_;
    // Game::MountManager& mount_manager_;
    Game::VIPManager& vip_manager_;
    Game::DungeonManager& dungeon_manager_;
    // TODO: 需要重构 - 使用 pqxx 而不是 libpq
    // Game::SignInManager& signin_manager_;
    // Game::MarriageManager& marriage_manager_;
    // Game::DialogueManager& dialogue_manager_;  // TODO: 待实现

    // ========== 移动系统 ==========

    Shared::Movement::MovementValidationConfig movement_config_;
    std::unique_ptr<Shared::Movement::MovementValidator> movement_validator_;
    std::unique_ptr<Shared::Movement::PositionBroadcaster> position_broadcaster_;

    // ========== 技能系统 ==========

    std::shared_ptr<Game::DamageCalculator> damage_calculator_;
    std::shared_ptr<Game::SpecialStateManager> buff_manager_;
    // TODO: SkillHandler 需要 refactoring，暂时注释掉
    // std::unique_ptr<Shared::Battle::SkillHandler> skill_handler_;

    // ========== 物品处理 ==========

    std::unique_ptr<Game::ItemHandler> item_handler_;

    // ========== 玩家管理 ==========

    std::unordered_map<uint64_t, uint64_t> players_;  // character_id -> session_id
    std::unordered_map<uint64_t, uint64_t> sessions_; // session_id -> character_id

    // ========== 连接管理 ==========

    // character_id -> Connection 映射，用于广播消息
    std::unordered_map<uint64_t, Core::Network::Connection::Ptr> character_connections_;

    // ========== 服务器配置 ==========

    uint32_t tick_rate_ = 100;  // 服务器tick率(ms)
    std::chrono::steady_clock::time_point last_update_;

    // ========== 怪物系统 ==========

    // 怪物模板缓存 (monster_id -> MonsterTemplate)
    std::unordered_map<uint32_t, MonsterTemplate> monster_templates_;

    // ========== 网络层 ==========

    std::unique_ptr<Core::Network::IOContext> io_context_;
    std::unique_ptr<Core::Network::Acceptor> acceptor_;
    std::thread io_thread_;
    std::atomic<bool> io_running_{false};
    uint16_t listen_port_ = 9001;

    // Work guard to keep io_context running
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;

    // ========== 辅助方法 ==========

    /**
     * @brief 加载怪物系统
     * @return 是否加载成功
     */
    bool LoadMonsterSystem();

    /**
     * @brief 处理服务器tick
     */
    void ProcessTick();

    /**
     * @brief 广播服务器状态
     */
    void BroadcastServerStatus();

    /**
     * @brief 处理玩家登录请求
     */
    void HandlePlayerLogin(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理角色移动请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleMoveRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 发送移动响应
     * @param conn 客户端连接
     * @param character_id 角色ID
     * @param result_code 结果码 (0=成功, 1=错误, 2=其他错误)
     * @param x 新位置X坐标
     * @param y 新位置Y坐标
     * @param z 新位置Z坐标
     * @param direction 朝向
     */
    void SendMoveResponse(Core::Network::Connection::Ptr conn, int64_t character_id, int32_t result_code, float x, float y, float z, uint16_t direction);

    /**
     * @brief 处理技能释放请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleSkillCastRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 发送技能释放响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=冷却中, 2=内力不足, 3=技能不存在, 4=目标无效)
     * @param skill_id 技能ID
     * @param damage 造成伤害（如果有）
     */
    void SendSkillCastResponse(Core::Network::Connection::Ptr conn, int32_t result_code, uint32_t skill_id, uint32_t damage);

    // ========== 状态同步 ==========

    /**
     * @brief 广播角色移动给附近玩家
     * @param character_id 移动的角色ID
     * @param x 新位置X
     * @param y 新位置Y
     * @param z 新位置Z
     * @param direction 朝向
     * @param exclude_conn 要排除的连接（不发送给自己）
     */
    void BroadcastCharacterMovement(
        uint64_t character_id,
        float x, float y, float z,
        uint16_t direction,
        Core::Network::Connection::Ptr exclude_conn = nullptr
    );

    /**
     * @brief 广播角色属性变化（HP/MP等）
     * @param character_id 角色ID
     * @param hp 当前HP
     * @param max_hp 最大HP
     * @param mp 当前MP
     * @param max_mp 最大MP
     * @param exclude_conn 要排除的连接
     */
    void BroadcastCharacterStats(
        uint64_t character_id,
        uint32_t hp, uint32_t max_hp,
        uint32_t mp, uint32_t max_mp,
        Core::Network::Connection::Ptr exclude_conn = nullptr
    );

    // ========== 死亡处理 ==========

    /**
     * @brief 处理角色死亡
     * @param character_id 死亡的角色ID
     * @param killer_id 凶手ID（0表示非玩家杀死）
     * @conn 死亡角色的连接
     */
    void HandleCharacterDeath(uint64_t character_id, uint64_t killer_id, Core::Network::Connection::Ptr conn);

    /**
     * @brief 广播角色死亡给附近玩家
     * @param character_id 死亡的角色ID
     * @param killer_id 凶手ID
     * @param exclude_conn 要排除的连接
     */
    void BroadcastCharacterDeath(
        uint64_t character_id,
        uint64_t killer_id,
        Core::Network::Connection::Ptr exclude_conn = nullptr
    );

    /**
     * @brief 处理角色复活
     * @param character_id 复活的角色ID
     * @param revive_type 复活类型 (0=城镇复活, 1=原地复活)
     * @param conn 客户端连接
     * @return 是否成功
     */
    bool HandleCharacterRevive(uint64_t character_id, uint8_t revive_type, Core::Network::Connection::Ptr conn);

    /**
     * @brief 发送复活响应给客户端
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=失败, 2=冷却中, 3=未死亡)
     * @param character_id 角色ID
     * @param revive_x 复活位置X
     * @param revive_y 复活位置Y
     * @param revive_z 复活位置Z
     */
    void SendReviveResponse(
        Core::Network::Connection::Ptr conn,
        int32_t result_code,
        uint64_t character_id,
        float revive_x,
        float revive_y,
        float revive_z
    );

    // ========== 拾取系统 ==========

    /**
     * @brief 处理拾取请求
     * @param character_id 拾取角色ID
     * @param loot_id 掉落物ID
     * @param conn 客户端连接
     * @return 是否成功
     */
    bool HandleLootRequest(uint64_t character_id, uint64_t loot_id, Core::Network::Connection::Ptr conn);

    /**
     * @brief 发送拾取响应
     * @param conn 客户端连接
     * @param result_code 结果码 (0=成功, 1=失败, 2=不存在, 3=无权拾取, 4=背包满)
     * @param loot_id 掉落物ID
     */
    void SendLootResponse(
        Core::Network::Connection::Ptr conn,
        int32_t result_code,
        uint64_t loot_id
    );

    // ========== 位置更新 ==========

    /**
     * @brief 发送位置更新
     * @param target_id 目标ID
     * @param data 数据包
     */
    void SendPositionUpdate(uint64_t target_id, const std::vector<uint8_t>& data);

    /**
     * @brief 处理位置更新请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandlePositionUpdate(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    // ========== 物品操作处理 ==========

    /**
     * @brief 处理装备物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleEquipItemRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理卸下装备请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleUnequipItemRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理使用消耗品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleUseConsumableRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理丢弃物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleDropItemRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理整理物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleMergeItemRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);

    /**
     * @brief 处理分割物品请求
     * @param conn 客户端连接
     * @param buffer 消息缓冲区
     */
    void HandleSplitItemRequest(Core::Network::Connection::Ptr conn, const Core::Network::ByteBuffer& buffer);
};

} // namespace MapServer
} // namespace Murim
