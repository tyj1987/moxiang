#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstring>

namespace Murim {
namespace Core {
namespace Network {

class ByteBuffer : public std::enable_shared_from_this<ByteBuffer> {
public:
    using Ptr = std::shared_ptr<ByteBuffer>;

    explicit ByteBuffer(size_t capacity = 1024);

    ByteBuffer(const uint8_t* data, size_t size);

    const uint8_t* GetData() const { return buffer_.data(); }
    const uint8_t* GetWriteData() const { return buffer_.data() + write_pos_; }
    size_t GetSize() const { return write_pos_; }

    void Write(const void* data, size_t size);
    void WriteUInt16(uint16_t value);
    void WriteUInt32(uint32_t value);
    void WriteUInt64(uint64_t value);
    void WriteString(const std::string& str);

    size_t Read(void* data, size_t size);
    uint16_t ReadUInt16();
    uint32_t ReadUInt32();
    uint64_t ReadUInt64();
    std::string ReadString(size_t length);

    void Clear() {
        write_pos_ = 0;
        read_pos_ = 0;
    }

    size_t GetReadPosition() const {
        return read_pos_;
    }

    void SetReadPosition(size_t pos) {
        read_pos_ = pos;
    }

    size_t GetReadableSize() const {
        return write_pos_ - read_pos_;
    }

    size_t GetWritableSize() const {
        return buffer_.size() - write_pos_;
    }

    void Reserve(size_t new_capacity);

private:
    std::vector<uint8_t> buffer_;
    size_t read_pos_;
    size_t write_pos_;
};

} // namespace Network
} // namespace Core
} // namespace Murim
