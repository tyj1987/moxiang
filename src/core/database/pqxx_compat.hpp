#pragma once

// 使用 libpq C API 包装器（替代 libpqxx）
#include "libpq_wrapper.hpp"

#include <string>
#include <cstdint>
#include <optional>

// 为了兼容性，定义 pqxx 命名空间别名
namespace pqxx {
    using Connection = Murim::Core::Database::Connection;
    using Result = Murim::Core::Database::Result;
}

namespace Murim {
namespace Core {
namespace Database {
namespace Compat {

/**
 * @brief 从结果集中获取字段值（兼容 libpqxx 接口）
 */
template<typename T>
inline std::optional<T> GetField(Result& result, int row, const std::string& col_name) {
    return result.Get<T>(row, col_name);
}

/**
 * @brief 获取可选字段值（处理 NULL）
 */
template<typename T>
inline std::optional<T> GetOptionalField(Result& result, int row, const std::string& col_name) {
    return result.Get<T>(row, col_name);
}

/**
 * @brief 获取字段值或默认值
 */
template<typename T>
inline T GetFieldOrDefault(Result& result, int row, const std::string& col_name, const T& default_value) {
    auto val = result.Get<T>(row, col_name);
    return val.has_value() ? val.value() : default_value;
}

/**
 * @brief 获取字符串字段（NULL 返回空字符串）
 */
inline std::string GetStringField(Result& result, int row, const std::string& col_name) {
    auto val = result.Get<std::string>(row, col_name);
    return val.has_value() ? val.value() : "";
}

} // namespace Compat
} // namespace Database
} // namespace Core
} // namespace Murim
