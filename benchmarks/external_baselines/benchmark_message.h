#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <span>

namespace orbitqueue::benchmark {

inline constexpr std::size_t max_payload_size = 256;
inline constexpr std::size_t payload_header_size = 32;

struct PayloadHeader {
    std::uint64_t global_id{};
    std::uint64_t local_sequence{};
    std::uint32_t producer_id{};
    std::uint32_t payload_size{};
    std::uint64_t checksum{};
};

static_assert(sizeof(PayloadHeader) == payload_header_size);

struct Payload {
    std::array<std::byte, max_payload_size> bytes{};
    std::uint32_t size{};
};

[[nodiscard]] inline std::uint64_t mix(const PayloadHeader& header) noexcept {
    auto value = header.global_id ^
                 (static_cast<std::uint64_t>(header.producer_id) << 32U) ^
                 header.local_sequence ^ 0x4f52424954515545ULL;
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

[[nodiscard]] inline Payload make_payload(
    const std::uint32_t payload_size,
    const std::uint32_t producer_id,
    const std::uint64_t local_sequence,
    const std::uint64_t global_id) {
    Payload payload;
    payload.size = payload_size;
    PayloadHeader header{
        global_id, local_sequence, producer_id, payload_size, 0};
    header.checksum = mix(header);
    std::memcpy(payload.bytes.data(), &header, sizeof(header));
    std::mt19937_64 random(header.checksum);
    for (std::size_t index = sizeof(header); index < payload.size; ++index) {
        payload.bytes[index] = static_cast<std::byte>(random() & 0xffU);
    }
    return payload;
}

[[nodiscard]] inline std::span<const std::byte> readable(
    const Payload& payload) noexcept {
    return {payload.bytes.data(), payload.size};
}

[[nodiscard]] inline std::span<std::byte> writable(Payload& payload) noexcept {
    return {payload.bytes.data(), payload.size};
}

[[nodiscard]] inline PayloadHeader read_header(const Payload& payload) noexcept {
    PayloadHeader header;
    if (payload.size >= sizeof(header)) {
        std::memcpy(&header, payload.bytes.data(), sizeof(header));
    }
    return header;
}

[[nodiscard]] inline bool valid_payload(
    const Payload& payload,
    const std::uint32_t expected_size) {
    if (payload.size != expected_size || payload.size < sizeof(PayloadHeader)) {
        return false;
    }
    const auto header = read_header(payload);
    if (header.payload_size != payload.size || header.global_id == 0 ||
        header.local_sequence == 0 || header.checksum != mix(header)) {
        return false;
    }
    const auto expected = make_payload(
        payload.size, header.producer_id, header.local_sequence, header.global_id);
    return std::equal(
        readable(payload).begin(), readable(payload).end(),
        readable(expected).begin(), readable(expected).end());
}

} // namespace orbitqueue::benchmark
