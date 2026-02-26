#include "ByteBuffer.hpp"

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
#endif

namespace Murim {
namespace Core {
namespace Network {

ByteBuffer::ByteBuffer(size_t capacity)
    : buffer_(capacity), read_pos_(0), write_pos_(0) {
}

ByteBuffer::ByteBuffer(const uint8_t* data, size_t size)
    : buffer_(data, data + size), read_pos_(0), write_pos_(size) {
}

void ByteBuffer::Write(const void* data, size_t size) {
    if (GetWritableSize() < size) {
        Reserve(write_pos_ + size);
    }

    std::memcpy(buffer_.data() + write_pos_, data, size);
    write_pos_ += size;
}

void ByteBuffer::WriteUInt16(uint16_t value) {
    uint16_t net_value = htons(value);
    Write(&net_value, sizeof(net_value));
}

void ByteBuffer::WriteUInt32(uint32_t value) {
    uint32_t net_value = htonl(value);
    Write(&net_value, sizeof(net_value));
}

void ByteBuffer::WriteUInt64(uint64_t value) {
    Write(&value, sizeof(value));
}

void ByteBuffer::WriteString(const std::string& str) {
    Write(str.data(), str.size());
}

size_t ByteBuffer::Read(void* data, size_t size) {
    size_t readable = GetReadableSize();
    if (readable < size) {
        size = readable;
    }

    std::memcpy(data, buffer_.data() + read_pos_, size);
    read_pos_ += size;
    return size;
}

uint16_t ByteBuffer::ReadUInt16() {
    uint16_t value = 0;
    Read(&value, sizeof(value));
    return ntohs(value);
}

uint32_t ByteBuffer::ReadUInt32() {
    uint32_t value = 0;
    Read(&value, sizeof(value));
    return ntohl(value);
}

uint64_t ByteBuffer::ReadUInt64() {
    uint64_t value = 0;
    Read(&value, sizeof(value));
    return value;
}

std::string ByteBuffer::ReadString(size_t length) {
    if (GetReadableSize() < length) {
        length = GetReadableSize();
    }

    std::string str(reinterpret_cast<char*>(buffer_.data() + read_pos_), length);
    read_pos_ += length;
    return str;
}

void ByteBuffer::Reserve(size_t new_capacity) {
    if (new_capacity <= buffer_.size()) {
        return;
    }

    buffer_.resize(new_capacity);
}

} // namespace Network
} // namespace Core
} // namespace Murim
