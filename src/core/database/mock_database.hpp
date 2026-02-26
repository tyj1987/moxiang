#pragma once

// Mock PostgreSQL implementation for quick compilation
// This allows the server to compile and run without PostgreSQL installed
// Replace with real PostgreSQL when needed

#ifndef USE_REAL_POSTGRESQL
    #undef PQXX_HPP
    #define MOCK_POSTGRESQL 1

    #include <string>
    #include <vector>
    #include <optional>
    #include <unordered_map>
    #include <cstdint>

    namespace pqxx {

    // Mock result class
    class row {
    public:
        class field {
        private:
            std::string value_;
            bool is_null_;

        public:
            field() : value_(""), is_null_(true) {}
            field(const std::string& v) : value_(v), is_null_(false) {}

            bool is_null() const { return is_null_; }
            const char* c_str() const { return value_.c_str(); }
            std::string as(std::string) const { return value_; }
            int as(int) const { return std::stoi(value_); }
            long as(long) const { return std::stol(value_); }
            long long as(long long) const { return std::stoll(value_); }
            unsigned int as(unsigned int) const { return std::stoul(value_); }
            unsigned long as(unsigned long) const { return std::stoul(value_); }
            unsigned long long as(unsigned long long) const { return std::stoull(value_); }
            float as(float) const { return std::stof(value_); }
            double as(double) const { return std::stod(value_); }
        };

        field operator[](const std::string& col_name) const {
            auto it = data_.find(col_name);
            if (it != data_.end()) {
                return field(it->second);
            }
            return field();
        }

        field operator[](int index) const {
            // Simple implementation - just return first value
            if (!data_.empty()) {
                return field(data_.begin()->second);
            }
            return field();
        }

        void add_column(const std::string& name, const std::string& value) {
            data_[name] = value;
        }

    private:
        std::unordered_map<std::string, std::string> data_;
    };

    class result {
    private:
        std::vector<row> rows_;

    public:
        using const_iterator = std::vector<row>::const_iterator;

        const_iterator begin() const { return rows_.begin(); }
        const_iterator end() const { return rows_.end(); }
        size_t size() const { return rows_.size(); }
        bool empty() const { return rows_.empty(); }

        void add_row(const row& r) { rows_.push_back(r); }
    };

    // Mock work class
    class work {
    public:
        work() {}
        void commit() {}
        void rollback() {}
    };

    } // namespace pqxx

    // Mock connection
    using Connection = std::shared_ptr<int>;

#endif // MOCK_POSTGRESQL
