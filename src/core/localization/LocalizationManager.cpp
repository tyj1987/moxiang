/**
 * LocalizationManager.cpp
 *
 * 本地化管理器实现
 */

#include "LocalizationManager.hpp"
#include "../spdlog_wrapper.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Murim {
namespace Core {
namespace Localization {

LocalizationManager& LocalizationManager::Instance() {
    static LocalizationManager instance;
    return instance;
}

bool LocalizationManager::Initialize(Language default_language, const std::string& locale_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    spdlog::info("Initializing LocalizationManager...");
    spdlog::info("Locale path: {}", locale_path);
    spdlog::info("Default language: {}", GetLanguageName(default_language));

    locale_path_ = locale_path;
    current_language_ = default_language;

    // 加载默认语言包
    if (!LoadLanguagePack(default_language)) {
        spdlog::warn("Failed to load default language pack, using embedded translations");
    }

    // 加载内置的中文翻译
    LoadBuiltinTranslations();

    spdlog::info("LocalizationManager initialized successfully");
    return true;
}

void LocalizationManager::SetLanguage(Language language) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (language == current_language_) {
        return;
    }

    spdlog::info("Switching language from {} to {}",
                 GetLanguageName(current_language_),
                 GetLanguageName(language));

    current_language_ = language;

    // 如果该语言没有加载,尝试加载
    if (translations_[language].empty()) {
        LoadLanguagePack(language);
    }
}

bool LocalizationManager::LoadLanguagePack(Language language) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::string lang_code = GetLanguageCode(language);
    std::string file_path = locale_path_ + "locale_" + lang_code + ".ini";

    spdlog::info("Loading language pack: {} ({})", file_path, lang_code);

    std::ifstream file(file_path);
    if (!file.is_open()) {
        spdlog::warn("Language pack file not found: {}", file_path);
        return false;
    }

    std::string line;
    std::string current_category;
    int line_number = 0;
    int loaded_count = 0;

    while (std::getline(file, line)) {
        ++line_number;

        // 跳过空行和注释
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // 解析类别 [Category]
        if (line[0] == '[' && line.back() == ']') {
            current_category = line.substr(1, line.length() - 2);
            continue;
        }

        // 解析键值对 key=value
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 确定文本类别
            TextCategory category = TextCategory::kSystem;
            if (current_category == "System") category = TextCategory::kSystem;
            else if (current_category == "Error") category = TextCategory::kError;
            else if (current_category == "UI") category = TextCategory::kUI;
            else if (current_category == "Item") category = TextCategory::kItem;
            else if (current_category == "Skill") category = TextCategory::kSkill;
            else if (current_category == "Quest") category = TextCategory::kQuest;
            else if (current_category == "NPC") category = TextCategory::kNPC;
            else if (current_category == "Notification") category = TextCategory::kNotification;
            else if (current_category == "Log") category = TextCategory::kLog;

            // 存储翻译
            translations_[language][category][key] = value;
            ++loaded_count;
        }
    }

    file.close();

    spdlog::info("Language pack loaded successfully: {} translations", loaded_count);
    return true;
}

std::string LocalizationManager::Translate(const std::string& key, TextCategory category) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    // 查找当前语言的翻译
    auto lang_it = translations_.find(current_language_);
    if (lang_it != translations_.end()) {
        auto cat_it = lang_it->second.find(category);
        if (cat_it != lang_it->second.end()) {
            auto key_it = cat_it->second.find(key);
            if (key_it != cat_it->second.end()) {
                return key_it->second;
            }
        }
    }

    // 如果当前语言没有翻译,尝试使用中文作为后备
    if (current_language_ != Language::kChinese) {
        auto zh_lang_it = translations_.find(Language::kChinese);
        if (zh_lang_it != translations_.end()) {
            auto cat_it = zh_lang_it->second.find(category);
            if (cat_it != zh_lang_it->second.end()) {
                auto key_it = cat_it->second.find(key);
                if (key_it != cat_it->second.end()) {
                    spdlog::warn("Translation missing for key: {}, category: {}, using Chinese fallback", key, GetCategoryName(category));
                    return key_it->second;
                }
            }
        }
    }

    // 如果都没有,返回键本身
    spdlog::warn("Translation not found: key={}, category={}", key, GetCategoryName(category));
    return key;
}

bool LocalizationManager::HasTranslation(const std::string& key, TextCategory category) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto lang_it = translations_.find(current_language_);
    if (lang_it != translations_.end()) {
        auto cat_it = lang_it->second.find(category);
        if (cat_it != lang_it->second.end()) {
            return cat_it->second.find(key) != cat_it->second.end();
        }
    }
    return false;
}

void LocalizationManager::AddTranslation(const std::string& key,
                                        const std::string& text,
                                        TextCategory category,
                                        Language language) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    translations_[language][category][key] = text;
}

void LocalizationManager::AddTranslations(const std::unordered_map<std::string, std::string>& translations,
                                         TextCategory category,
                                         Language language) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto& [key, text] : translations) {
        translations_[language][category][key] = text;
    }
}

std::vector<std::string> LocalizationManager::GetMissingTranslations(Language language, TextCategory category) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::vector<std::string> missing;

    // 获取中文的所有键(作为基准)
    auto zh_lang_it = translations_.find(Language::kChinese);
    if (zh_lang_it == translations_.end()) {
        return missing;
    }

    auto zh_cat_it = zh_lang_it->second.find(category);
    if (zh_cat_it == zh_lang_it->second.end()) {
        return missing;
    }

    // 检查目标语言是否包含所有键
    auto lang_it = translations_.find(language);
    if (lang_it == translations_.end()) {
        // 目标语言完全没有翻译,所有键都缺失
        for (const auto& [key, _] : zh_cat_it->second) {
            missing.push_back(key);
        }
        return missing;
    }

    auto cat_it = lang_it->second.find(category);
    if (cat_it == lang_it->second.end()) {
        // 目标语言的该类别完全没有翻译
        for (const auto& [key, _] : zh_cat_it->second) {
            missing.push_back(key);
        }
        return missing;
    }

    // 找出缺失的键
    for (const auto& [key, _] : zh_cat_it->second) {
        if (cat_it->second.find(key) == cat_it->second.end()) {
            missing.push_back(key);
        }
    }

    return missing;
}

bool LocalizationManager::ExportLanguagePack(Language language, const std::string& file_path) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    spdlog::info("Exporting language pack: {} -> {}", GetLanguageName(language), file_path);

    std::ofstream file(file_path);
    if (!file.is_open()) {
        spdlog::error("Failed to create language pack file: {}", file_path);
        return false;
    }

    // 写入文件头
    file << "# ========================================\n";
    file << "# " << GetLanguageName(language) << " Language Pack\n";
    file << "# ========================================\n\n";

    auto lang_it = translations_.find(language);
    if (lang_it == translations_.end()) {
        file.close();
        spdlog::warn("No translations found for language: {}", GetLanguageName(language));
        return true;
    }

    // 按类别导出
    std::vector<TextCategory> categories = {
        TextCategory::kSystem,
        TextCategory::kError,
        TextCategory::kUI,
        TextCategory::kItem,
        TextCategory::kSkill,
        TextCategory::kQuest,
        TextCategory::kNPC,
        TextCategory::kNotification,
        TextCategory::kLog
    };

    for (TextCategory category : categories) {
        auto cat_it = lang_it->second.find(category);
        if (cat_it == lang_it->second.end() || cat_it->second.empty()) {
            continue;
        }

        file << "[" << GetCategoryName(category) << "]\n";
        for (const auto& [key, text] : cat_it->second) {
            file << key << "=" << text << "\n";
        }
        file << "\n";
    }

    file.close();

    spdlog::info("Language pack exported successfully");
    return true;
}

bool LocalizationManager::ImportLanguagePack(const std::string& file_path, Language language) {
    return LoadLanguagePack(language);
}

std::unordered_map<TextCategory, size_t> LocalizationManager::GetTranslationStats(Language language) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::unordered_map<TextCategory, size_t> stats;

    auto lang_it = translations_.find(language);
    if (lang_it == translations_.end()) {
        return stats;
    }

    for (const auto& [category, keys] : lang_it->second) {
        stats[category] = keys.size();
    }

    return stats;
}

std::string LocalizationManager::GetCategoryName(TextCategory category) {
    switch (category) {
        case TextCategory::kSystem: return "System";
        case TextCategory::kError: return "Error";
        case TextCategory::kUI: return "UI";
        case TextCategory::kItem: return "Item";
        case TextCategory::kSkill: return "Skill";
        case TextCategory::kQuest: return "Quest";
        case TextCategory::kNPC: return "NPC";
        case TextCategory::kNotification: return "Notification";
        case TextCategory::kLog: return "Log";
        default: return "Unknown";
    }
}

void LocalizationManager::LoadBuiltinTranslations() {
    spdlog::info("Loading builtin Chinese translations...");

    // 系统消息
    AddTranslation("login.success", "登录成功", TextCategory::kSystem, Language::kChinese);
    AddTranslation("login.failed", "登录失败", TextCategory::kSystem, Language::kChinese);
    AddTranslation("logout.success", "登出成功", TextCategory::kSystem, Language::kChinese);
    AddTranslation("server.started", "服务器已启动", TextCategory::kSystem, Language::kChinese);
    AddTranslation("server.stopped", "服务器已停止", TextCategory::kSystem, Language::kChinese);

    // 错误消息
    AddTranslation("error.account_not_found", "账号不存在", TextCategory::kError, Language::kChinese);
    AddTranslation("error.wrong_password", "密码错误", TextCategory::kError, Language::kChinese);
    AddTranslation("error.account_banned", "账号已被封禁", TextCategory::kError, Language::kChinese);
    AddTranslation("error.character_not_found", "角色不存在", TextCategory::kError, Language::kChinese);
    AddTranslation("error.character_name_exists", "角色名已存在", TextCategory::kError, Language::kChinese);
    AddTranslation("error.database_error", "数据库错误", TextCategory::kError, Language::kChinese);
    AddTranslation("error.network_error", "网络错误", TextCategory::kError, Language::kChinese);
    AddTranslation("error.invalid_session", "无效的会话", TextCategory::kError, Language::kChinese);
    AddTranslation("error.server_full", "服务器已满", TextCategory::kError, Language::kChinese);

    // UI文本
    AddTranslation("ui.login", "登录", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.logout", "登出", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.username", "用户名", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.password", "密码", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.create_character", "创建角色", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.select_character", "选择角色", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.delete_character", "删除角色", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.enter_game", "进入游戏", TextCategory::kUI, Language::kChinese);
    AddTranslation("ui.quit", "退出", TextCategory::kUI, Language::kChinese);

    // 通知消息
    AddTranslation("notification.player_login", "%s 上线了", TextCategory::kNotification, Language::kChinese);
    AddTranslation("notification.player_logout", "%s 下线了", TextCategory::kNotification, Language::kChinese);
    AddTranslation("notification.friend_online", "好友 %s 上线了", TextCategory::kNotification, Language::kChinese);
    AddTranslation("notification.friend_offline", "好友 %s 下线了", TextCategory::kNotification, Language::kChinese);

    // 日志消息
    AddTranslation("log.player_login", "玩家登录: account_id={}, character_id={}, name={}", TextCategory::kLog, Language::kChinese);
    AddTranslation("log.player_logout", "玩家登出: character_id={}, name={}", TextCategory::kLog, Language::kChinese);
    AddTranslation("log.character_created", "角色创建成功: name={}, class={}", TextCategory::kLog, Language::kChinese);
    AddTranslation("log.skill_learned", "技能学习: character_id={}, skill_id={}", TextCategory::kLog, Language::kChinese);
    AddTranslation("log.item_obtained", "获得物品: character_id={}, item_id={}, quantity={}", TextCategory::kLog, Language::kChinese);
    AddTranslation("log.quest_completed", "任务完成: character_id={}, quest_id={}", TextCategory::kLog, Language::kChinese);

    spdlog::info("Builtin translations loaded successfully");
}

} // namespace Localization
} // namespace Core
} // namespace Murim
