#pragma once

#include <cstddef>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace test_support {

inline void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename T>
std::span<const std::byte> bytes_of(const T& value) noexcept {
    return std::as_bytes(std::span{&value, std::size_t{1}});
}

template <typename T>
std::span<std::byte> writable_bytes_of(T& value) noexcept {
    return std::as_writable_bytes(std::span{&value, std::size_t{1}});
}

} // namespace test_support
