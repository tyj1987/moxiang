/**
 * Language.hpp
 *
 * 语言支持定义
 */

#pragma once

#include <string>
#include <unordered_map>

namespace Murim {
namespace Core {
namespace Localization {

/**
 * 支持的语言
 */
enum class Language : uint8_t {
    kChinese = 0,      // 简体中文 (中国区)
    kTraditionalChinese = 1,  // 繁体中文 (台湾/香港)
    kEnglish = 2,       // 英语
    kKorean = 3,        // 韩语
    kJapanese = 4       // 日语
};

/**
 * 语言配置
 */
struct LanguageConfig {
    Language language;
    std::string code;           // ISO 639-1 语言代码
    std::string name;           // 语言名称
    std::string region;         // 地区代码
};

/**
 * 获取所有支持的语言
 * @return 语言列表
 */
inline std::unordered_map<Language, LanguageConfig> GetSupportedLanguages() {
    return {
        {Language::kChinese, {Language::kChinese, "zh-CN", "简体中文", "CN"}},
        {Language::kTraditionalChinese, {Language::kTraditionalChinese, "zh-TW", "繁體中文", "TW"}},
        {Language::kEnglish, {Language::kEnglish, "en-US", "English", "US"}},
        {Language::kKorean, {Language::kKorean, "ko-KR", "한국어", "KR"}},
        {Language::kJapanese, {Language::kJapanese, "ja-JP", "日本語", "JP"}}
    };
}

/**
 * 根据代码获取语言
 * @param code 语言代码 (如 "zh-CN")
 * @return 语言类型
 */
inline Language GetLanguageByCode(const std::string& code) {
    auto languages = GetSupportedLanguages();
    for (const auto& [lang, config] : languages) {
        if (config.code == code) {
            return lang;
        }
    }
    return Language::kChinese;  // 默认中文
}

/**
 * 获取语言代码
 * @param language 语言类型
 * @return 语言代码
 */
inline std::string GetLanguageCode(Language language) {
    auto languages = GetSupportedLanguages();
    auto it = languages.find(language);
    if (it != languages.end()) {
        return it->second.code;
    }
    return "zh-CN";
}

/**
 * 获取语言名称
 * @param language 语言类型
 * @return 语言名称
 */
inline std::string GetLanguageName(Language language) {
    auto languages = GetSupportedLanguages();
    auto it = languages.find(language);
    if (it != languages.end()) {
        return it->second.name;
    }
    return "简体中文";
}

} // namespace Localization
} // namespace Core
} // namespace Murim
