#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace orbitqueue::benchmark {

inline constexpr std::size_t default_capacity = 1024;
inline constexpr std::array<std::size_t, 3> default_consumer_matrix{1, 3, 10};

struct SequenceRange {
    std::uint64_t first{};
    std::uint64_t last{};
};

class SequenceTracker {
public:
    [[nodiscard]] bool record(const std::uint64_t sequence) {
        if (sequence == 0) {
            return false;
        }
        if (ranges_.empty() ||
            (ranges_.back().last != std::numeric_limits<std::uint64_t>::max() &&
             sequence > ranges_.back().last + 1)) {
            ranges_.push_back({sequence, sequence});
            return true;
        }
        if (ranges_.back().last != std::numeric_limits<std::uint64_t>::max() &&
            sequence == ranges_.back().last + 1) {
            ranges_.back().last = sequence;
            return true;
        }
        return false;
    }

    [[nodiscard]] const std::vector<SequenceRange>& ranges() const noexcept {
        return ranges_;
    }

private:
    std::vector<SequenceRange> ranges_;
};

[[nodiscard]] inline std::uint64_t count_unique_sequences(
    const std::vector<SequenceTracker>& trackers) {
    std::vector<SequenceRange> ranges;
    for (const auto& tracker : trackers) {
        ranges.insert(ranges.end(), tracker.ranges().begin(), tracker.ranges().end());
    }
    if (ranges.empty()) {
        return 0;
    }

    std::sort(ranges.begin(), ranges.end(), [](const auto& left, const auto& right) {
        return left.first < right.first ||
               (left.first == right.first && left.last < right.last);
    });

    auto current = ranges.front();
    std::uint64_t count = 0;
    for (std::size_t index = 1; index < ranges.size(); ++index) {
        const auto& range = ranges[index];
        if (current.last != std::numeric_limits<std::uint64_t>::max() &&
            range.first > current.last + 1) {
            count += current.last - current.first + 1;
            current = range;
        } else if (range.last > current.last) {
            current.last = range.last;
        }
    }
    return count + current.last - current.first + 1;
}

[[nodiscard]] inline std::string json_escape(const std::string_view value) {
    std::ostringstream output;
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec;
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

template <typename Value>
void append_json_array(std::ostringstream& output, const std::vector<Value>& values) {
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << values[index];
    }
    output << ']';
}

struct Result {
    std::string benchmark_group;
    std::string queue;
    std::string delivery_semantics;
    std::uint32_t trial{};
    std::size_t capacity{};
    std::size_t payload_size{};
    std::size_t producer_count{};
    std::size_t consumer_count{};
    std::uint64_t duration_ms{};
    std::uint64_t warmup_ms{};
    std::uint64_t messages_published{};
    std::uint64_t aggregate_reads{};
    std::uint64_t unique_sequences_verified{};
    std::uint64_t dropped_or_lagged{};
    std::uint64_t invalid_payloads{};
    std::uint64_t full_retries{};
    std::uint64_t empty_retries{};
    std::uint64_t validation_errors{};
    double throughput_messages_per_second{};
    double throughput_reads_per_second{};
    double throughput_bytes_per_second{};
    std::vector<std::uint64_t> consumer_reads;
    std::vector<double> consumer_reads_per_second;
    std::string build_type;
    std::string compiler;
    std::string git_commit;
    std::string timestamp;
    std::string notes;
};

[[nodiscard]] inline std::string to_json(const Result& result) {
    std::ostringstream output;
    output << std::setprecision(12)
           << "{\"benchmark_group\":\"" << json_escape(result.benchmark_group)
           << "\",\"queue\":\"" << json_escape(result.queue)
           << "\",\"delivery_semantics\":\""
           << json_escape(result.delivery_semantics)
           << "\",\"trial\":" << result.trial
           << ",\"capacity\":" << result.capacity
           << ",\"payload_size\":" << result.payload_size
           << ",\"payload_bytes\":" << result.payload_size
           << ",\"producer_count\":" << result.producer_count
           << ",\"producers\":" << result.producer_count
           << ",\"consumer_count\":" << result.consumer_count
           << ",\"consumers\":" << result.consumer_count
           << ",\"duration_ms\":" << result.duration_ms
           << ",\"duration_seconds\":"
           << (static_cast<double>(result.duration_ms) / 1000.0)
           << ",\"warmup_ms\":" << result.warmup_ms
           << ",\"messages_published\":" << result.messages_published
           << ",\"messages_produced\":" << result.messages_published
           << ",\"aggregate_reads\":" << result.aggregate_reads
           << ",\"messages_consumed\":" << result.aggregate_reads
           << ",\"unique_sequences_verified\":"
           << result.unique_sequences_verified
           << ",\"dropped_or_lagged\":" << result.dropped_or_lagged
           << ",\"invalid_payloads\":" << result.invalid_payloads
           << ",\"full_retries\":" << result.full_retries
           << ",\"producer_failed_tries\":" << result.full_retries
           << ",\"empty_retries\":" << result.empty_retries
           << ",\"consumer_failed_tries\":" << result.empty_retries
           << ",\"consumer_lagged_count\":" << result.dropped_or_lagged
           << ",\"validation_errors\":" << result.validation_errors
           << ",\"validation\":\""
           << (result.validation_errors == 0 ? "passed" : "failed") << '"'
           << ",\"throughput_messages_per_second\":"
           << result.throughput_messages_per_second
           << ",\"messages_per_second\":"
           << result.throughput_messages_per_second
           << ",\"throughput_reads_per_second\":"
           << result.throughput_reads_per_second
           << ",\"bytes_per_second\":"
           << result.throughput_bytes_per_second;
    output << ",\"consumer_reads\":";
    append_json_array(output, result.consumer_reads);
    output << ",\"consumer_reads_per_second\":";
    append_json_array(output, result.consumer_reads_per_second);
    output << ",\"build_type\":\"" << json_escape(result.build_type)
           << "\",\"compiler\":\"" << json_escape(result.compiler)
           << "\",\"git_commit\":\"" << json_escape(result.git_commit)
           << "\",\"timestamp\":\"" << json_escape(result.timestamp)
           << "\",\"notes\":\"" << json_escape(result.notes) << "\"}";
    return output.str();
}

} // namespace orbitqueue::benchmark
