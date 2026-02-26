#pragma once

// Mock pqxx implementation for development/testing
// This allows compilation without PostgreSQL installed

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace pqxx {

// Mock connection class
class connection {
public:
    connection() {}
    connection(const std::string& conn_str) {}
    bool is_open() const { return true; }
    void close() {}
};

// Mock result class
class field {
private:
    std::string value_;
    bool is_null_;

public:
    field() : value_(""), is_null_(true) {}
    field(const std::string& v) : value_(v), is_null_(false) {}

    bool is_null() const { return is_null_; }
    const char* c_str() const { return value_.c_str(); }

    template<typename T>
    T as() const {
        if constexpr (std::is_same_v<T, std::string>) {
            return value_;
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(std::stoll(value_));
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(std::stod(value_));
        }
        return T();
    }
};

class row {
private:
    std::vector<std::pair<std::string, field>> fields_;

public:
    field operator[](const std::string& col_name) const {
        for (const auto& f : fields_) {
            if (f.first == col_name) {
                return f.second;
            }
        }
        return field();
    }

    field operator[](size_t index) const {
        if (index < fields_.size()) {
            return fields_[index].second;
        }
        return field();
    }

    bool empty() const { return fields_.empty(); }
    size_t size() const { return fields_.size(); }

    void add_field(const std::string& name, const std::string& value) {
        fields_.push_back({name, field(value)});
    }
};

class result {
private:
    std::vector<row> rows_;
    size_t affected_rows_count = 0;

public:
    using const_iterator = std::vector<row>::const_iterator;

    const_iterator begin() const { return rows_.begin(); }
    const_iterator end() const { return rows_.end(); }
    size_t size() const { return rows_.size(); }
    bool empty() const { return rows_.empty(); }
    size_t affected_rows() const { return affected_rows_count; }

    row operator[](size_t index) const {
        if (index < rows_.size()) {
            return rows_[index];
        }
        return row();
    }

    void add_row(const row& r) { rows_.push_back(r); }
    void set_affected_rows(size_t count) { affected_rows_count = count; }
};

class work {
public:
    work() {}
    work(connection&) {}
    work(connection*) {}
    void commit() {}
    void rollback() {}
    result exec(const std::string& query) {
        return result();
    }
    template<typename... Args>
    result exec(const std::string& query, Args&&... args) {
        return result();
    }
};

class nontransaction : public work {
public:
    nontransaction() {}
    nontransaction(connection&) {}
};

} // namespace pqxx
