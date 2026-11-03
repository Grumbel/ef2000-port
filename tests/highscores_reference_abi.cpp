// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Test-only C ABI for byte-identical guest/native differential comparisons.
// Never exposed as a game runtime or stored as part of the shipped package.

#include "../runtime/ef2000_highscores.hpp"

#include <cstdint>
#include <cstring>

using namespace ef2000::highscores;

extern "C" int ef2000_test_highscore_insert(std::uint8_t* state_bytes,
                                             const std::uint8_t* name_bytes,
                                             std::uint8_t difficulty) {
    State state{};
    Name name{};
    std::memcpy(&state, state_bytes, sizeof(state));
    std::memcpy(name.data(), name_bytes, name.size());
    const int result = insert(state, name, difficulty);
    std::memcpy(state_bytes, &state, sizeof(state));
    return result;
}

extern "C" int ef2000_test_highscore_exceeds_last(const std::uint8_t* state_bytes) {
    State state{};
    std::memcpy(&state, state_bytes, sizeof(state));
    return exceeds_last_score(state) ? 1 : 0;
}

extern "C" int ef2000_test_highscore_defaults(std::uint8_t* state_bytes,
                                               const std::uint8_t* names_bytes,
                                               const std::int32_t* scores,
                                               const std::uint16_t* random_values) {
    State state{};
    RandomNames names{};
    DefaultScores templates{};
    std::memcpy(&state, state_bytes, sizeof(state));
    std::memcpy(&names, names_bytes, sizeof(names));
    std::memcpy(&templates, scores, sizeof(templates));
    std::size_t index = 0;
    const bool success = initialize_defaults(state, names, templates,
                                             [&]() { return random_values[index++]; });
    std::memcpy(state_bytes, &state, sizeof(state));
    return success ? static_cast<int>(index) : -1;
}

extern "C" void ef2000_test_highscore_encode(const std::uint8_t* state_bytes,
                                              std::uint8_t* file_bytes) {
    State state{};
    std::memcpy(&state, state_bytes, sizeof(state));
    const auto file = encode_file(state);
    std::memcpy(file_bytes, file.data(), file.size());
}

extern "C" int ef2000_test_highscore_decode(std::uint8_t* state_bytes,
                                              const std::uint8_t* file_bytes,
                                              std::size_t length) {
    State state{};
    std::memcpy(&state, state_bytes, sizeof(state));
    const bool valid = decode_file(state, file_bytes, length);
    std::memcpy(state_bytes, &state, sizeof(state));
    return valid ? 1 : 0;
}
