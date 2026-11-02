// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ef2000_highscores.hpp"

#include <algorithm>
#include <cstring>

namespace ef2000::highscores {
namespace {

std::uint32_t read_u32(const std::uint8_t* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
           (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

void write_u32(std::uint8_t* bytes, std::uint32_t value) {
    for (int n = 0; n != 4; ++n) {
        bytes[n] = static_cast<std::uint8_t>(value >> (8 * n));
    }
}

std::int32_t as_signed(std::uint32_t bits) {
    std::int32_t result;
    static_assert(sizeof(result) == sizeof(bits));
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

bool terminated(const Name& name) {
    return std::find(name.begin(), name.end(), std::uint8_t{0}) != name.end();
}

// Guest strcpy at 0x9d749 includes the terminating NUL, not the 20-byte pad.
void copy_name(Name& destination, const Name& source) {
    const auto end = std::find(source.begin(), source.end(), std::uint8_t{0});
    std::copy(source.begin(), end + 1, destination.begin());
}

} // namespace

std::int32_t score(const Record& record) {
    return as_signed(read_u32(record.score_le.data()));
}

void set_score(Record& record, std::int32_t value) {
    write_u32(record.score_le.data(), static_cast<std::uint32_t>(value));
}

std::int32_t current_score(const State& state) {
    return as_signed(read_u32(state.current_score_le.data()));
}

void set_current_score(State& state, std::int32_t value) {
    write_u32(state.current_score_le.data(), static_cast<std::uint32_t>(value));
}

bool exceeds_last_score(const State& state) {
    return current_score(state) > score(state.records.back());
}

int insert(State& state, const Name& name, std::uint8_t difficulty) {
    if (!terminated(name)) return -2;
    const auto value = current_score(state);
    std::size_t position = 0;
    for (; position < kRecordCount; ++position) {
        // The guest executes DEC EAX then signed CMP/JLE at 0x435e..0x4367.
        // Preserve 32-bit wraparound, including stored INT_MIN -> INT_MAX.
        const auto threshold = as_signed(
            static_cast<std::uint32_t>(score(state.records[position])) - 1u);
        if (value > threshold) break;
    }
    if (position == kRecordCount) return -1;
    for (std::size_t slot = kRecordCount - 1; slot > position; --slot) {
        if (!terminated(state.records[slot - 1].name)) return -2;
    }
    for (std::size_t slot = kRecordCount - 1; slot > position; --slot) {
        copy_name(state.records[slot].name, state.records[slot - 1].name);
        state.records[slot].difficulty = state.records[slot - 1].difficulty;
        state.records[slot].score_le = state.records[slot - 1].score_le;
    }
    copy_name(state.records[position].name, name);
    state.records[position].difficulty = difficulty;
    set_score(state.records[position], value);
    return static_cast<int>(position);
}

bool initialize_defaults(State& state, const RandomNames& pool,
                         const DefaultScores& template_scores,
                         const Random16& next_random) {
    if (std::any_of(pool.begin(), pool.end(),
                    [](const Name& name) { return !terminated(name); })) return false;
    for (std::size_t slot = 0; slot < kRecordCount; ++slot) {
        const auto name_index = next_random() % kRandomNameCount;
        copy_name(state.records[slot].name, pool[name_index]);
        state.records[slot].difficulty = static_cast<std::uint8_t>(
            next_random() % 3 + 1);
        set_score(state.records[slot], template_scores[slot]);
    }
    return true;
}

std::uint32_t original_checksum(const State& state) {
    return static_cast<std::uint32_t>(state.records[0].name[0]) * kTableBytes;
}

std::array<std::uint8_t, kFileBytes> encode_file(const State& state) {
    std::array<std::uint8_t, kFileBytes> result{};
    write_u32(result.data(), original_checksum(state));
    std::memcpy(result.data() + 4, state.records.data(), kTableBytes);
    return result;
}

bool decode_file(State& state, const std::uint8_t* data, std::size_t size) {
    if (size < kFileBytes || data == nullptr) return false;
    const auto saved_checksum = read_u32(data);
    std::memcpy(state.records.data(), data + 4, kTableBytes);
    return saved_checksum == original_checksum(state);
}

} // namespace ef2000::highscores
