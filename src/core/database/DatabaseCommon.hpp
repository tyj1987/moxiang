#pragma once

#include "DatabasePool.hpp"
#include "QueryExecutor.hpp"
#include "../spdlog_wrapper.hpp"

namespace Murim {
namespace Core {
namespace Database {

// 全局类型别名 - 简化所有DAO文件
using ConnectionPool = ConnectionPool;
using QueryExecutor = QueryExecutor;

} // namespace Core
} // namespace Murim
