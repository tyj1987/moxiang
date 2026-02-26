#pragma once

#include <string>
#include <optional>
#include <cstdint>
#include <stdexcept>

// Minimal libpqxx mock for compilation testing
namespace pqxx {

// Forward declarations
class row;
class field;

class connection {
public:
    connection(const std::string&) {}
    ~connection() {}
    bool is_open() const { return false; }
    void close() {}
};

class field {
public:
    field() = default;

    // Mock field access
    template<typename T>
    T as() const {
        return T{};
    }

    const char* c_str() const {
        return "";
    }

    bool is_null() const {
        return true;
    }

    std::string name() const {
        return "";
    }

    size_t size() const {
        return 0;
    }
};

class row {
public:
    row() = default;

    // Mock row type
    size_t size() const { return 0; }

    // Implement operator[] for column access by name
    field operator[](const std::string& col_name) const {
        return field{};
    }

    // Implement operator[] for column access by index
    field operator[](size_t index) const {
        return field{};
    }

    // Helper methods
    bool empty() const { return true; }

    size_t column_number(const std::string& col_name) const {
        return static_cast<size_t>(-1);
    }
};

class result {
public:
    result() = default;
    size_t size() const { return 0; }
    size_t affected_rows() const { return 0; }
    bool empty() const { return true; }

    // Mock iterator
    class const_iterator {
    public:
        const_iterator() = default;
        const_iterator& operator++() { return *this; }
        const_iterator operator++(int) { return *this; }
        bool operator!=(const const_iterator&) const { return false; }
        bool operator==(const const_iterator&) const { return true; }

        // Dereference operator
        row operator*() const {
            return row{};
        }

        // Arrow operator
        const row* operator->() const {
            static row dummy_row;
            return &dummy_row;
        }
    };

    const_iterator begin() const { return const_iterator(); }
    const_iterator end() const { const_iterator it; return it; }

    // Index operator for result
    row operator[](size_t index) const {
        return row{};
    }

    row front() const {
        return row{};
    }
};

class work {
public:
    work(connection&) {}
    ~work() {}

    void commit() {}
    void abort() {}

    // Mock exec methods
    result exec(const std::string& sql) {
        return result{};
    }

    result exec_params(const std::string& sql) {
        return result{};
    }

    template<typename... Args>
    result exec(const std::string& sql, Args&&... args) {
        return result{};
    }

    template<typename... Args>
    result exec_params(const std::string& sql, Args&&... args) {
        return result{};
    }
};

// Mock exception types
struct sql_error : public std::runtime_error {
    sql_error(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace pqxx
