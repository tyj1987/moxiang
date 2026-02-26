/**
 * LocalizationManager.hpp
 *
 * 本地化管理器 - 管理多语言翻译
 * 支持动态加载语言包、文本翻译、格式化等功能
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "Language.hpp"

namespace Murim {
namespace Core {
namespace Localization {

/**
 * 文本类别
 */
enum class TextCategory : uint8_t {
    kSystem = 0,        // 系统消息
    kError = 1,         // 错误消息
    kUI = 2,            // UI文本
    kItem = 3,          // 物品名称
    kSkill = 4,         // 技能名称
    kQuest = 5,         // 任务文本
    kNPC = 6,           // NPC对话
    kNotification = 7,  // 通知消息
    kLog = 8            // 日志消息
};

/**
 * 本地化管理器 - 单例
 */
class LocalizationManager {
public:
    /**
     * @brief 获取单例实例
     */
    static LocalizationManager& Instance();

    /**
     * @brief 初始化本地化系统
     * @param default_language 默认语言
     * @param locale_path 语言包路径
     * @return 是否初始化成功
     */
    bool Initialize(Language default_language = Language::kChinese,
                   const std::string& locale_path = "locales/");

    /**
     * @brief 设置当前语言
     * @param language 语言类型
     */
    void SetLanguage(Language language);

    /**
     * @brief 获取当前语言
     * @return 当前语言类型
     */
    Language GetLanguage() const { return current_language_; }

    /**
     * @brief 加载语言包
     * @param language 要加载的语言
     * @return 是否加载成功
     */
    bool LoadLanguagePack(Language language);

    /**
     * @brief 翻译文本
     * @param key 翻译键
     * @param category 文本类别
     * @return 翻译后的文本
     */
    std::string Translate(const std::string& key,
                         TextCategory category = TextCategory::kSystem);

    /**
     * @brief 翻译并格式化文本
     * @param key 翻译键
     * @param category 文本类别
     * @param args 格式化参数
     * @return 翻译并格式化后的文本
     */
    template<typename... Args>
    std::string TranslateFormat(const std::string& key,
                               TextCategory category,
                               Args... args);

    /**
     * @brief 检查翻译是否存在
     * @param key 翻译键
     * @param category 文本类别
     * @return 是否存在翻译
     */
    bool HasTranslation(const std::string& key,
                       TextCategory category = TextCategory::kSystem);

    /**
     * @brief 添加翻译
     * @param key 翻译键
     * @param text 翻译文本
     * @param category 文本类别
     * @param language 语言 (默认为当前语言)
     */
    void AddTranslation(const std::string& key,
                       const std::string& text,
                       TextCategory category,
                       Language language = Language::kChinese);

    /**
     * @brief 批量添加翻译
     * @param translations 翻译映射
     * @param category 文本类别
     * @param language 语言
     */
    void AddTranslations(const std::unordered_map<std::string, std::string>& translations,
                         TextCategory category,
                         Language language = Language::kChinese);

    /**
     * @brief 获取缺失的翻译列表
     * @param language 要检查的语言
     * @param category 文本类别 (可选)
     * @return 缺失的翻译键列表
     */
    std::vector<std::string> GetMissingTranslations(Language language,
                                                    TextCategory category = TextCategory::kSystem);

    /**
     * @brief 导出语言包到文件
     * @param language 要导出的语言
     * @param file_path 导出文件路径
     * @return 是否导出成功
     */
    bool ExportLanguagePack(Language language, const std::string& file_path);

    /**
     * @brief 从文件导入语言包
     * @param file_path 语言包文件路径
     * @param language 语言类型
     * @return 是否导入成功
     */
    bool ImportLanguagePack(const std::string& file_path, Language language);

    /**
     * @brief 获取翻译统计信息
     * @param language 语言类型
     * @return 统计信息映射 (类别 -> 翻译数量)
     */
    std::unordered_map<TextCategory, size_t> GetTranslationStats(Language language);

    // ========== 便捷方法 ==========

    /**
     * @brief 翻译系统消息
     */
    std::string System(const std::string& key) {
        return Translate(key, TextCategory::kSystem);
    }

    /**
     * @brief 翻译错误消息
     */
    std::string Error(const std::string& key) {
        return Translate(key, TextCategory::kError);
    }

    /**
     * @brief 翻译UI文本
     */
    std::string UI(const std::string& key) {
        return Translate(key, TextCategory::kUI);
    }

    /**
     * @brief 翻译物品名称
     */
    std::string Item(const std::string& key) {
        return Translate(key, TextCategory::kItem);
    }

    /**
     * @brief 翻译技能名称
     */
    std::string Skill(const std::string& key) {
        return Translate(key, TextCategory::kSkill);
    }

    /**
     * @brief 翻译任务文本
     */
    std::string Quest(const std::string& key) {
        return Translate(key, TextCategory::kQuest);
    }

    /**
     * @brief 翻译NPC对话
     */
    std::string NPC(const std::string& key) {
        return Translate(key, TextCategory::kNPC);
    }

    /**
     * @brief 翻译通知消息
     */
    std::string Notification(const std::string& key) {
        return Translate(key, TextCategory::kNotification);
    }

    /**
     * @brief 翻译日志消息
     */
    std::string Log(const std::string& key) {
        return Translate(key, TextCategory::kLog);
    }

private:
    LocalizationManager() = default;
    ~LocalizationManager() = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    // ========== 数据结构 ==========

    // 翻译存储: language -> category -> key -> text
    std::unordered_map<Language,
        std::unordered_map<TextCategory,
            std::unordered_map<std::string, std::string>>> translations_;

    Language current_language_ = Language::kChinese;  // 当前语言
    std::string locale_path_ = "locales/";            // 语言包路径
    mutable std::recursive_mutex mutex_;               // 线程安全锁（递归锁，避免死锁）

    // ========== 辅助方法 ==========

    /**
     * @brief 格式化字符串
     */
    template<typename... Args>
    std::string FormatString(const std::string& format, Args... args);

    /**
     * @brief 获取类别名称
     */
    std::string GetCategoryName(TextCategory category);

    /**
     * @brief 加载内置翻译
     */
    void LoadBuiltinTranslations();
};

// 模板方法实现
template<typename... Args>
std::string LocalizationManager::TranslateFormat(const std::string& key,
                                                 TextCategory category,
                                                 Args... args) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    std::string text = Translate(key, category);
    return FormatString(text, args...);
}

template<typename... Args>
std::string LocalizationManager::FormatString(const std::string& format, Args... args) {
    // 简单的字符串格式化实现
    // 在实际项目中可以使用 fmt 库或其他格式化库

    size_t buffer_size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    std::vector<char> buffer(buffer_size);
    std::snprintf(buffer.data(), buffer_size, format.c_str(), args...);
    return std::string(buffer.data(), buffer.size() - 1);
}

} // namespace Localization
} // namespace Core
} // namespace Murim
