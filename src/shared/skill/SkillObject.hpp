// MurimServer - SkillObject System
// 技能对象系统 - 支持持续性技能、区域技能、状态效果等
// 对应legacy: skillobject_server.h, SkillObjectAttachUnit.h, SkillObjectSingleUnit.h等

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include "battle/BattleTypes.hpp"

namespace Murim {
namespace Game {

// ========== Forward Declarations ==========
class SkillInfo;
class GameObject;
class SkillObjectTargetList;
class SkillObjectTerminator;
class SkillObjectFirstUnit;
class SkillObjectSingleUnit;
class SkillObjectStateUnit;
class SkillObjectAttachUnit;

// ========== Constants ==========
constexpr int SO_DESTROYOBJECT = -1;
constexpr int SO_OK = 0;
constexpr int MUGONG_USE_EXP = 100;

// ========== Skill Result Kind ==========
enum class SkillResultKind : uint8_t {
    None = 0,
    Positive = 1,    // 正面效果（友方）
    Negative = 2     // 负面效果（敌方）
};

// ========== Skill Target List Entry ==========
struct STList {
    uint64_t object_id;
    GameObject* object;
    SkillResultKind target_kind;

    STList() : object_id(0), object(nullptr), target_kind(SkillResultKind::None) {}
    STList(GameObject* obj, SkillResultKind kind)
        : object_id(0), object(obj), target_kind(kind) {
        if (obj) object_id = obj->GetObjectId();
    }
};

// ========== Main Target Info ==========
struct MainTargetInfo {
    enum class TargetKind : uint8_t {
        ObjectId = 0,
        Position = 1
    };

    TargetKind kind;
    union {
        uint64_t target_id;
        struct {
            float x, y, z;
        } target_pos;
    };

    MainTargetInfo() : kind(TargetKind::ObjectId), target_id(0) {}

    void SetTarget(uint64_t id) {
        kind = TargetKind::ObjectId;
        target_id = id;
    }

    void SetTarget(float x, float y, float z) {
        kind = TargetKind::Position;
        target_pos.x = x;
        target_pos.y = y;
        target_pos.z = z;
    }
};

// ========== Skill Object Info ==========
struct SkillObjectInfo {
    uint32_t skill_object_idx;   // 技能对象索引
    uint16_t skill_idx;          // 技能索引
    uint16_t skill_level;        // 技能等级

    struct {
        float x, y, z;
    } position;

    uint32_t start_time;         // 开始时间
    uint8_t direction;           // 方向
    uint64_t operator_id;        // 施法者ID

    MainTargetInfo main_target;  // 主目标

    uint32_t battle_id;          // 战场ID
    uint8_t battle_kind;         // 战场类型
    uint16_t option;             // 选项索引

    SkillObjectInfo()
        : skill_object_idx(0), skill_idx(0), skill_level(0),
          position{0, 0, 0}, start_time(0), direction(0),
          operator_id(0), battle_id(0), battle_kind(0), option(0) {}
};

// ========== Skill Object Terminator ==========
// 技能对象终止器 - 判断技能对象何时结束
class SkillObjectTerminator {
public:
    SkillObjectTerminator() = default;
    virtual ~SkillObjectTerminator() = default;

    virtual void Init(const SkillInfo* skill_info, uint16_t skill_level) = 0;
    virtual bool CheckTerminate(const SkillObjectInfo& info) = 0;
};

// ========== Terminator: Time-based ==========
class SkillObjectTerminator_Time : public SkillObjectTerminator {
private:
    uint32_t duration_ms_;

public:
    SkillObjectTerminator_Time() : duration_ms_(0) {}
    virtual ~SkillObjectTerminator_Time() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level) override;
    bool CheckTerminate(const SkillObjectInfo& info) override;
};

// ========== Skill Object Target List ==========
// 技能对象目标列表 - 管理技能影响的目标
class SkillObjectTargetList {
protected:
    SkillObject* skill_object_;
    std::map<uint64_t, STList> target_table_;

    bool position_set_head_;
    uint16_t positive_target_type_;
    uint16_t negative_target_type_;
    uint16_t skill_range_;
    uint16_t skill_area_idx_;

    mutable auto current_iterator_;

    virtual void DoRelease() {}
    virtual void DoInitTargetList() {}

public:
    SkillObjectTargetList();
    virtual ~SkillObjectTargetList();

    void SetSkillObject(SkillObject* obj) { skill_object_ = obj; }

    // 更新目标列表
    uint32_t UpdateTargetList(GameObject* center_object);

    // 遍历目标
    void SetPositionHead();
    STList* GetNextTargetList();
    GameObject* GetNextTarget();
    bool GetNextTarget(SkillResultKind kind, GameObject** out_object, SkillResultKind* out_target_kind);
    bool GetNextNegativeTarget(GameObject** out_object, SkillResultKind* out_kind);
    bool GetNextPositiveTarget(GameObject** out_object, SkillResultKind* out_kind);

    // 目标查询
    bool IsInTargetList(GameObject* object) const;
    bool IsInTargetList(uint64_t object_id) const;
    SkillResultKind GetTargetKind(uint64_t object_id) const;

    // 目标管理
    void InitTargetList(const std::vector<GameObject*>& initial_targets,
                       GameObject* additional_target,
                       const MainTargetInfo& main_target);
    void Release();

    // 添加/移除目标
    virtual uint8_t AddTargetObject(GameObject* object, SkillResultKind kind);
    virtual uint8_t RemoveTargetObject(uint64_t object_id);

    // 目标验证（虚函数，子类可重写）
    virtual bool IsValidTarget(GameObject* object) const;

    // 范围检测（纯虚函数，子类实现）
    virtual bool IsInTargetArea(GameObject* object) const = 0;
    virtual void SetSkillObjectPosition(float x, float y, float z) = 0;
    virtual void GetSkillObjectPosition(float* x, float* y, float* z) const {}

    size_t GetTargetNum() const { return target_table_.size(); }
};

// ========== Target List: Area ==========
class SkillObjectTargetList_Area : public SkillObjectTargetList {
private:
    float area_x_, area_y_, area_z_;
    float range_;

public:
    SkillObjectTargetList_Area() : area_x_(0), area_y_(0), area_z_(0), range_(0) {}
    virtual ~SkillObjectTargetList_Area() = default;

    bool IsInTargetArea(GameObject* object) const override;
    void SetSkillObjectPosition(float x, float y, float z) override {
        area_x_ = x;
        area_y_ = y;
        area_z_ = z;
    }
    void GetSkillObjectPosition(float* x, float* y, float* z) const override {
        *x = area_x_;
        *y = area_y_;
        *z = area_z_;
    }
};

// ========== Target List: Circle ==========
class SkillObjectTargetList_Circle : public SkillObjectTargetList {
private:
    float center_x_, center_y_, center_z_;
    float radius_;

public:
    SkillObjectTargetList_Circle() : center_x_(0), center_y_(0), center_z_(0), radius_(0) {}
    virtual ~SkillObjectTargetList_Circle() = default;

    bool IsInTargetArea(GameObject* object) const override;
    void SetSkillObjectPosition(float x, float y, float z) override {
        center_x_ = x;
        center_y_ = y;
        center_z_ = z;
    }
};

// ========== Target List: Fixed ==========
class SkillObjectTargetList_Fixed : public SkillObjectTargetList {
public:
    SkillObjectTargetList_Fixed() = default;
    virtual ~SkillObjectTargetList_Fixed() = default;

    bool IsInTargetArea(GameObject* object) const override { return true; }
    void SetSkillObjectPosition(float x, float y, float z) override {}
};

// ========== Target List: One ==========
class SkillObjectTargetList_One : public SkillObjectTargetList {
private:
    uint64_t target_id_;

public:
    SkillObjectTargetList_One() : target_id_(0) {}
    virtual ~SkillObjectTargetList_One() = default;

    bool IsInTargetArea(GameObject* object) const override;
    void SetSkillObjectPosition(float x, float y, float z) override {}
};

// ========== Skill Object First Unit ==========
// 技能对象的首次效果单元 - 技能施放时的即时效果
class SkillObjectFirstUnit {
protected:
    uint16_t attrib_;                    // 属性
    float recover_state_abnormal_;       // 恢复异常状态概率
    uint16_t stun_time_;                 // 眩晕时间
    float stun_rate_;                    // 眩晕概率

    float dispel_attack_feel_rate_;     // 驱散攻击感概率

public:
    SkillObjectFirstUnit() = default;
    virtual ~SkillObjectFirstUnit() = default;

    virtual void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr);

    // 执行首次效果
    virtual int ExecuteFirstUnit(GameObject* operator_obj,
                                  SkillObjectTargetList* target_list,
                                  float skill_tree_amp);
    virtual void FirstUnitResult() {}
};

// ========== First Unit: Attack ==========
class SkillObjectFirstUnit_Attack : public SkillObjectFirstUnit {
private:
    float physical_attack_;
    uint16_t attrib_;
    uint16_t attrib_attack_;
    float attack_attack_rate_;

public:
    SkillObjectFirstUnit_Attack() : physical_attack_(0), attrib_(0), attrib_attack_(0), attack_attack_rate_(0) {}
    virtual ~SkillObjectFirstUnit_Attack() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    int ExecuteFirstUnit(GameObject* operator_obj,
                         SkillObjectTargetList* target_list,
                         float skill_tree_amp) override;
};

// ========== First Unit: Recover ==========
class SkillObjectFirstUnit_Recover : public SkillObjectFirstUnit {
private:
    float recover_hp_;
    float recover_mp_;

public:
    SkillObjectFirstUnit_Recover() : recover_hp_(0), recover_mp_(0) {}
    virtual ~SkillObjectFirstUnit_Recover() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    int ExecuteFirstUnit(GameObject* operator_obj,
                         SkillObjectTargetList* target_list,
                         float skill_tree_amp) override;
};

// ========== First Unit: Job ==========
class SkillObjectFirstUnit_Job : public SkillObjectFirstUnit {
public:
    SkillObjectFirstUnit_Job() = default;
    virtual ~SkillObjectFirstUnit_Job() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
};

// ========== First Unit: Single Special State ==========
class SkillObjectFirstUnit_SingleSpecialState : public SkillObjectFirstUnit {
private:
    uint32_t state_id_;
    float duration_;

public:
    SkillObjectFirstUnit_SingleSpecialState() : state_id_(0), duration_(0) {}
    virtual ~SkillObjectFirstUnit_SingleSpecialState() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    int ExecuteFirstUnit(GameObject* operator_obj,
                         SkillObjectTargetList* target_list,
                         float skill_tree_amp) override;
};

// ========== Skill Object Single Unit ==========
// 技能对象的周期性效果单元 - 持续伤害、治疗等
class SkillObjectSingleUnit {
protected:
    uint32_t single_unit_num_;    // 单元编号
    uint32_t interval_;           // 执行间隔(ms)
    uint32_t operate_count_;      // 已执行次数

    SkillResultKind pn_target_;   // 0:正面 1:负面
    SkillObject* skill_object_;

    virtual void DoInit(const SkillInfo* skill_info, uint16_t skill_level) {}

public:
    SkillObjectSingleUnit(uint32_t num)
        : single_unit_num_(num), interval_(0), operate_count_(0),
          pn_target_(SkillResultKind::Positive), skill_object_(nullptr) {}

    virtual ~SkillObjectSingleUnit() = default;

    SkillResultKind GetPNTarget() const { return pn_target_; }
    void SetSkillObject(SkillObject* obj) { skill_object_ = obj; }
    uint32_t GetSingleUnitNum() const { return single_unit_num_; }

    virtual void Init(const SkillInfo* skill_info, uint16_t skill_level);
    virtual void Release() {}

    void Update(SkillObjectInfo* info, SkillObjectTargetList* target_list, float skill_tree_amp);
    virtual void Operate(SkillObjectInfo* info, SkillObjectTargetList* target_list, float skill_tree_amp) = 0;
};

// ========== Single Unit: Attack ==========
class SkillObjectSingleUnit_Attack : public SkillObjectSingleUnit {
private:
    float physical_attack_;
    uint16_t attrib_;
    uint16_t attrib_attack_;
    float attack_attack_rate_;

    void DoInit(const SkillInfo* skill_info, uint16_t skill_level) override;

public:
    SkillObjectSingleUnit_Attack(uint32_t num) : SkillObjectSingleUnit(num),
        physical_attack_(0), attrib_(0), attrib_attack_(0), attack_attack_rate_(0) {}
    virtual ~SkillObjectSingleUnit_Attack() = default;

    void Operate(SkillObjectInfo* info, SkillObjectTargetList* target_list, float skill_tree_amp) override;
};

// ========== Single Unit: Heal ==========
class SkillObjectSingleUnit_Heal : public SkillObjectSingleUnit {
private:
    float heal_amount_;

public:
    SkillObjectSingleUnit_Heal(uint32_t num) : SkillObjectSingleUnit(num), heal_amount_(0) {}
    virtual ~SkillObjectSingleUnit_Heal() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level) override;
    void Operate(SkillObjectInfo* info, SkillObjectTargetList* target_list, float skill_tree_amp) override;
};

// ========== Single Unit: Recharge ==========
class SkillObjectSingleUnit_Recharge : public SkillObjectSingleUnit {
private:
    float recharge_mp_;

public:
    SkillObjectSingleUnit_Recharge(uint32_t num) : SkillObjectSingleUnit(num), recharge_mp_(0) {}
    virtual ~SkillObjectSingleUnit_Recharge() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level) override;
    void Operate(SkillObjectInfo* info, SkillObjectTargetList* target_list, float skill_tree_amp) override;
};

// ========== Skill Object State Unit ==========
// 技能对象的状态单元 - 持续性状态效果（Buff/Debuff）
class SkillObjectStateUnit {
protected:
    SkillObject* skill_object_;
    SkillResultKind pn_target_;

public:
    SkillObjectStateUnit() : skill_object_(nullptr), pn_target_(SkillResultKind::Positive) {}
    virtual ~SkillObjectStateUnit() = default;

    SkillResultKind GetPNTarget() const { return pn_target_; }
    void SetSkillObject(SkillObject* obj) { skill_object_ = obj; }

    virtual void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) = 0;

    void StartState(SkillObjectTargetList* target_list);
    void EndState(SkillObjectTargetList* target_list);
    void StartState(GameObject* object, SkillResultKind kind);
    void EndState(GameObject* object, SkillResultKind kind);

    virtual void DoStartState(GameObject* object, GameObject* skill_operator = nullptr) = 0;
    virtual void DoEndState(GameObject* object) = 0;
};

// ========== State Unit: Status Attach ==========
class SkillObjectStateUnit_StatusAttach : public SkillObjectStateUnit {
private:
    uint32_t state_id_;
    float duration_;

public:
    SkillObjectStateUnit_StatusAttach() : state_id_(0), duration_(0) {}
    virtual ~SkillObjectStateUnit_StatusAttach() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    void DoStartState(GameObject* object, GameObject* skill_operator = nullptr) override;
    void DoEndState(GameObject* object) override;
};

// ========== State Unit: Tie Up ==========
class SkillObjectStateUnit_TieUp : public SkillObjectStateUnit {
private:
    float duration_;

public:
    SkillObjectStateUnit_TieUp() : duration_(0) {}
    virtual ~SkillObjectStateUnit_TieUp() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    void DoStartState(GameObject* object, GameObject* skill_operator = nullptr) override;
    void DoEndState(GameObject* object) override;
};

// ========== State Unit: Dummy State ==========
class SkillObjectStateUnit_DummyState : public SkillObjectStateUnit {
public:
    SkillObjectStateUnit_DummyState() = default;
    virtual ~SkillObjectStateUnit_DummyState() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    void DoStartState(GameObject* object, GameObject* skill_operator = nullptr) override;
    void DoEndState(GameObject* object) override;
};

// ========== State Unit: Amplified Power ==========
class SkillObjectStateUnit_AmplifiedPower : public SkillObjectStateUnit {
private:
    float power_rate_;

public:
    SkillObjectStateUnit_AmplifiedPower() : power_rate_(0) {}
    virtual ~SkillObjectStateUnit_AmplifiedPower() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
    void DoStartState(GameObject* object, GameObject* skill_operator = nullptr) override;
    void DoEndState(GameObject* object) override;
};

// ========== Skill Object Attach Unit ==========
// 技能对象的附加单元 - 附加在目标身上的属性修正
class SkillObjectAttachUnit {
protected:
    int attach_unit_kind_;

public:
    enum AttachUnitKind {
        Unknown = 0,
        Dodge = 1
    };

    SkillObjectAttachUnit() : attach_unit_kind_(AttachUnitKind::Unknown) {}
    virtual ~SkillObjectAttachUnit() = default;

    virtual void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) = 0;

    int GetAttachUnitKind() const { return attach_unit_kind_; }
};

// ========== Attach Unit: Attack Up ==========
class SkillObjectAttachUnit_AttackUp : public SkillObjectAttachUnit {
private:
    float phy_attack_up_;

public:
    SkillObjectAttachUnit_AttackUp() : phy_attack_up_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_AttackUp() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetPhyAttackUp() const { return phy_attack_up_; }
};

// ========== Attach Unit: Attack Down ==========
class SkillObjectAttachUnit_AttackDown : public SkillObjectAttachUnit {
private:
    float attack_attack_down_;
    float phy_attack_down_;

public:
    SkillObjectAttachUnit_AttackDown() : attack_attack_down_(0), phy_attack_down_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_AttackDown() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetAttackAttackDown() const { return attack_attack_down_; }
    float GetPhyAttackDown() const { return phy_attack_down_; }
};

// ========== Attach Unit: Defence ==========
class SkillObjectAttachUnit_Defence : public SkillObjectAttachUnit {
private:
    float defence_up_;

public:
    SkillObjectAttachUnit_Defence() : defence_up_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_Defence() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetDefenceUp() const { return defence_up_; }
};

// ========== Attach Unit: Move Speed ==========
class SkillObjectAttachUnit_MoveSpeed : public SkillObjectAttachUnit {
private:
    float move_speed_rate_;

public:
    SkillObjectAttachUnit_MoveSpeed() : move_speed_rate_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_MoveSpeed() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetMoveSpeedRate() const { return move_speed_rate_; }
};

// ========== Attach Unit: Vampiric ==========
class SkillObjectAttachUnit_Vampiric : public SkillObjectAttachUnit {
private:
    float vampiric_rate_;

public:
    SkillObjectAttachUnit_Vampiric() : vampiric_rate_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_Vampiric() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetVampiricRate() const { return vampiric_rate_; }
};

// ========== Attach Unit: Immune ==========
class SkillObjectAttachUnit_Immune : public SkillObjectAttachUnit {
public:
    SkillObjectAttachUnit_Immune() {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_Immune() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
};

// ========== Attach Unit: Counter Attack ==========
class SkillObjectAttachUnit_CounterAttack : public SkillObjectAttachUnit {
private:
    float counter_rate_;

public:
    SkillObjectAttachUnit_CounterAttack() : counter_rate_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_CounterAttack() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetCounterRate() const { return counter_rate_; }
};

// ========== Attach Unit: Max Life ==========
class SkillObjectAttachUnit_MaxLife : public SkillObjectAttachUnit {
private:
    float max_life_up_;

public:
    SkillObjectAttachUnit_MaxLife() : max_life_up_(0) {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_MaxLife() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetMaxLifeUp() const { return max_life_up_; }
};

// ========== Attach Unit: Dodge ==========
class SkillObjectAttachUnit_Dodge1 : public SkillObjectAttachUnit {
private:
    float dodge_rate_;

public:
    SkillObjectAttachUnit_Dodge1() : dodge_rate_(0) {
        attach_unit_kind_ = AttachUnitKind::Dodge;
    }
    virtual ~SkillObjectAttachUnit_Dodge1() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;

    float GetDodgeRate() const { return dodge_rate_; }
};

// ========== Attach Unit: Skip Effect ==========
class SkillObjectAttachUnit_SkipEffect : public SkillObjectAttachUnit {
public:
    SkillObjectAttachUnit_SkipEffect() {
        attach_unit_kind_ = AttachUnitKind::Unknown;
    }
    virtual ~SkillObjectAttachUnit_SkipEffect() = default;

    void Init(const SkillInfo* skill_info, uint16_t skill_level, GameObject* oper = nullptr) override;
};

// ========== Skill Object ==========
// 技能对象 - 持续性技能的核心类
class SkillObject {
protected:
    SkillObject(const SkillInfo* skill_info,
                std::unique_ptr<SkillObjectTerminator> terminator,
                std::unique_ptr<SkillObjectTargetList> target_list,
                std::unique_ptr<SkillObjectFirstUnit> first_unit);

    const SkillInfo* skill_info_;
    std::unique_ptr<SkillObjectTerminator> terminator_;
    std::unique_ptr<SkillObjectTargetList> target_list_;
    std::unique_ptr<SkillObjectFirstUnit> first_unit_;

    std::vector<std::unique_ptr<SkillObjectSingleUnit>> single_unit_list_;
    std::vector<std::unique_ptr<SkillObjectStateUnit>> state_unit_list_;
    std::vector<std::unique_ptr<SkillObjectAttachUnit>> attach_unit_list_;

    uint16_t positive_target_type_;
    uint16_t negative_target_type_;

    uint64_t operator_id_;

    SkillObjectInfo skill_object_info_;

    void AddSingleUnit(std::unique_ptr<SkillObjectSingleUnit> unit);
    void AddStateUnit(std::unique_ptr<SkillObjectStateUnit> unit);
    void AddAttachUnit(std::unique_ptr<SkillObjectAttachUnit> unit);

    bool die_flag_;

    uint32_t max_life_;
    uint32_t cur_life_;

    float skill_tree_amp_;

    uint16_t option_index_;

    bool IsSame(SkillObject* other);

public:
    int release_kind;  // YH临时

    virtual ~SkillObject();

    // 初始化技能对象
    virtual void Init(const SkillObjectInfo* info,
                     const std::vector<GameObject*>& initial_targets,
                     float skill_tree_amp = 1.0f,
                     GameObject* main_target = nullptr);

    // 释放技能对象
    void Release();

    // 更新技能对象
    virtual uint32_t Update();

    // 更新目标列表
    virtual void UpdateTargetList(GameObject* object);

    // 添加/移除目标对象
    void AddTargetObject(GameObject* object);
    void RemoveTargetObject(uint64_t object_id);

    uint16_t GetSkillIdx() const { return skill_object_info_.skill_idx; }

    friend class SkillObjectFactory;

    // 设置消息
    void SetAddMsg(char* add_msg, uint16_t* msg_len, uint64_t receiver_id, bool is_login);
    void SetRemoveMsg(char* remove_msg, uint16_t* msg_len, uint64_t receiver_id);

    // 获取位置和方向
    void GetPosition(float* x, float* y, float* z) const;
    uint8_t GetDirectionIndex() const { return skill_object_info_.direction; }

    // 目标类型判断
    bool IsNegativeTarget(GameObject* object) const;
    bool IsPositiveTarget(GameObject* object) const;
    bool TargetTypeCheck(uint16_t target_type, GameObject* object) const;

    // 获取操作者和技能信息
    GameObject* GetOperator();
    const SkillInfo* GetSkillInfo() const { return skill_info_; }
    SkillObjectInfo* GetSkillObjectInfo() { return &skill_object_info_; }

    // 操作技能对象
    virtual bool Operate(GameObject* requestor, MainTargetInfo* main_target,
                        const std::vector<GameObject*>& t_list) { return false; }

    // 首次单元结果
    void SkillObjectFirstUnitResult();

    // 覆盖函数
    virtual void DoDie(GameObject* attacker);
    virtual uint32_t GetLife();
    virtual void SetLife(uint32_t life, bool send_msg = true);
    virtual uint32_t DoGetMaxLife();
};

// ========== Skill Object Factory ==========
class SkillObjectFactory {
public:
    static std::unique_ptr<SkillObject> MakeNewSkillObject(const SkillInfo* skill_info);
};

} // namespace Game
} // namespace Murim
