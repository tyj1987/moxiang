#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>
#include <vector>

namespace spdlog {

enum class level {
    trace,
    debug,
    info,
    warn,
    error,
    critical
};

inline void set_level(level l) {
}

namespace detail {
    inline void format_to(std::ostringstream& oss, const char* fmt) {
        oss << fmt;
    }

    template<typename T>
    inline auto format_arg(std::ostringstream& oss, T value) -> decltype(oss << value, void()) {
        oss << value;
    }

    template<typename T, typename... Args>
    inline void format_to(std::ostringstream& oss, const char* fmt, T arg, Args... args) {
        const char* start = fmt;

        while (*fmt) {
            if (*fmt == '{' && *(fmt + 1) == '}') {
                oss.write(start, fmt - start);

                format_arg(oss, arg);

                fmt += 2;

                format_to(oss, fmt, args...);
                return;
            }
            fmt++;
        }

        oss << start;
    }
}

// ========== fmt 命名空间模拟（支持 fmt::runtime 和 fmt::arg）==========
namespace fmt {
    // 模拟 fmt::arg - 用于命名参数
    template<typename T>
    struct arg {
        const char* name;
        T value;

        arg(const char* n, T v) : name(n), value(v) {}
    };

    // 运行时格式化字符串包装器
    struct runtime_format {
        const char* fmt_str;

        runtime_format(const char* fmt) : fmt_str(fmt) {}
        runtime_format(const std::string& fmt) : fmt_str(fmt.c_str()) {}

        operator const char*() const { return fmt_str; }
    };

    // 模拟运行时格式化字符串
    inline runtime_format runtime(const char* fmt) {
        return runtime_format(fmt);
    }

    inline runtime_format runtime(const std::string& fmt) {
        return runtime_format(fmt);
    }

    // 简化版本 - 只保留字符串，忽略参数
    // 注意：这是一个最小化的实现，真正的 fmt 库会替换命名参数
    template<typename... Args>
    std::string format(const char* fmt_str, Args... args) {
        // 简化实现：直接返回格式字符串
        // 实际使用中应该替换 {} 占位符，但这里为了简化返回原始字符串
        return fmt_str;
    }
} // namespace fmt

// 处理 fmt::arg 参数的辅助函数
namespace detail {
    // 从 fmt::arg 中提取值
    template<typename T>
    inline auto extract_value(fmt::arg<T> arg) -> decltype(arg.value) {
        return arg.value;
    }

    // 用于处理 fmt::runtime + fmt::arg 参数的格式化函数
    template<typename... Args>
    inline void format_with_args(std::ostringstream& oss, const char* fmt, Args... args) {
        // 简化实现：忽略命名参数名称，按顺序替换 {}
        format_to(oss, fmt, extract_value(args)...);
    }
}

// 接受 std::string (来自 fmt::runtime) 的重载
template<typename... Args>
inline void debug(const std::string& fmt, Args... args) {
    #ifndef NDEBUG
    std::ostringstream oss;
    detail::format_with_args(oss, fmt.c_str(), args...);
    std::cout << "[DEBUG] " << oss.str() << std::endl;
    #endif
}

template<typename... Args>
inline void debug(const char* fmt, Args... args) {
    #ifndef NDEBUG
    std::ostringstream oss;
    detail::format_to(oss, fmt, args...);
    std::cout << oss.str() << std::endl;
    #endif
}

template<typename... Args>
inline void info(const char* fmt, Args... args) {
    std::ostringstream oss;
    detail::format_to(oss, fmt, args...);
    std::cout << "[INFO] " << oss.str() << std::endl;
}

template<typename... Args>
inline void trace(const char* fmt, Args... args) {
    #ifndef NDEBUG
    std::ostringstream oss;
    detail::format_to(oss, fmt, args...);
    std::cout << oss.str() << std::endl;
    #endif
}

template<typename... Args>
inline void warn(const char* fmt, Args... args) {
    std::ostringstream oss;
    detail::format_to(oss, fmt, args...);
    std::cerr << oss.str() << std::endl;
}

template<typename... Args>
inline void error(const char* fmt, Args... args) {
    std::ostringstream oss;
    detail::format_to(oss, fmt, args...);
    std::cerr << oss.str() << std::endl;
}

template<typename... Args>
inline void critical(const char* fmt, Args... args) {
    std::ostringstream oss;
    detail::format_to(oss, fmt, args...);
    std::cerr << oss.str() << std::endl;
}

} // namespace spdlog
