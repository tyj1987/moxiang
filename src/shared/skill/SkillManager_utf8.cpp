#include "Skill.hpp"
#include "core/network/NotificationService.hpp"
#include "core/spdlog_wrapper.hpp"
#include <unordered_map>
#include <algorithm>

using ConnectionPool = Murim::Core::Database::ConnectionPool;
using QueryExecutor = Murim::Core::Database::QueryExecutor;

namespace Murim {
namespace Game {

SkillManager& SkillManager::Instance() {
    static SkillManager instance;
    return instance;
}

// ========== 鎶€鑳藉姞锟iconv: illegal input sequence at position 317
