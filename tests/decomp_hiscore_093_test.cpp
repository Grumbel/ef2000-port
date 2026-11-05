// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

#include "ef_hiscore_093.h"
#include "ef2000_highscores.hpp"

extern "C" {
#include "ef_globals.h"
}

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace ef2000::highscores;

extern "C" {
int32_t g_hiscore_current = 0;
ef_hiscore_entry g_hiscore_table[10] = {};
}

static std::array<std::uint8_t, 0x500000> ram{};
static std::vector<std::uint16_t> numbers;
static std::size_t next_number = 0;
static unsigned seeds = 0;
static std::vector<std::uint8_t> file_data;
static std::size_t read_cursor = 0;
static bool file_exists = true;
static bool open_succeeds = true;
static unsigned file_opens = 0;
static unsigned file_closes = 0;
static unsigned ui_refreshes = 0;
static bool window_exists = true;
static std::array<unsigned, 3> icon_counts{10, 10, 10};
static unsigned icon_starts = 0;
static std::vector<std::string> ui_events;

extern "C" void *ef_guest_memory_ptr(ef_addr32 address)
{
    assert(address < ram.size());
    return &ram[address];
}

extern "C" void hiscore_seed_093(void) { ++seeds; }
extern "C" std::uint16_t hiscore_random_093(void)
{
    assert(next_number < numbers.size());
    return numbers[next_number++];
}
extern "C" int hiscore_file_exists_093(ef_addr32 filename)
{
    assert(filename == 0x17f694u);
    return file_exists;
}
extern "C" std::int32_t hiscore_file_open_093(ef_addr32 filename, int writing)
{
    assert(filename == 0x17f694u);
    ++file_opens;
    if (!open_succeeds) return -1;
    read_cursor = 0;
    if (writing) file_data.clear();
    return 23;
}
extern "C" void hiscore_file_read_093(std::int32_t handle, void *out, std::uint32_t length)
{
    assert(handle == 23 && file_data.size() >= read_cursor + length);
    std::memcpy(out, file_data.data() + read_cursor, length);
    read_cursor += length;
}
extern "C" void hiscore_file_write_093(std::int32_t handle,
                                         const void *src, std::uint32_t length)
{
    assert(handle == 23);
    auto ptr = static_cast<const std::uint8_t *>(src);
    file_data.insert(file_data.end(), ptr, ptr + length);
}
extern "C" void hiscore_file_close_093(std::int32_t handle)
{
    assert(handle == 23);
    ++file_closes;
}
extern "C" int hiscore_window_exists_093(void)
{
    ++ui_refreshes;
    return window_exists;
}
extern "C" ef_addr32 hiscore_first_icon_093(unsigned column)
{
    assert(column < 3);
    ++icon_starts;
    return icon_counts[column] ? 0x300000u + 0x1000u * column : 0;
}
extern "C" void hiscore_icon_text_093(ef_addr32 icon, const char *text)
{
    ui_events.push_back(std::to_string(icon) + ":" + text);
}
extern "C" void hiscore_invalid_level_093(ef_addr32 message)
{
    assert(message == 0x17f664u);
    ui_events.emplace_back("invalid");
}

static void reset(void)
{
    ram.fill(0);
    numbers.clear();
    next_number = 0;
    seeds = 0;
    file_data.clear();
    read_cursor = 0;
    file_exists = open_succeeds = window_exists = true;
    file_opens = file_closes = ui_refreshes = icon_starts = 0;
    icon_counts = {10, 10, 10};
    ui_events.clear();
    std::memset(g_hiscore_table, 0, sizeof(g_hiscore_table));
    g_hiscore_current = 0;
}

static void set_state(const State& state)
{
    std::memcpy(&g_hiscore_current, state.current_score_le.data(), 4);
    std::memcpy(g_hiscore_table, state.records.data(), kTableBytes);
}

static void check_state(const State& expected)
{
    assert(std::memcmp(&g_hiscore_current, expected.current_score_le.data(), 4) == 0);
    assert(std::memcmp(g_hiscore_table, expected.records.data(), kTableBytes) == 0);
}

static State random_state(std::mt19937& rng)
{
    State state{};
    set_current_score(state, static_cast<std::int32_t>(rng()));
    for (unsigned i = 0; i < 10; ++i) {
        auto& record = state.records[i];
        record.name.fill(static_cast<std::uint8_t>(rng()));
        std::string label = "Pilot" + std::to_string(i);
        std::copy(label.begin(), label.end(), record.name.begin());
        record.name[label.size()] = 0;
        record.difficulty = static_cast<std::uint8_t>(rng() % 3u + 1u);
        set_score(record, static_cast<std::int32_t>(rng()));
    }
    return state;
}

static void set_defaults_source(RandomNames& pool, DefaultScores& scores)
{
    for (unsigned i = 0; i < kRandomNameCount; ++i) {
        pool[i].fill(0x77);
        std::string label = "Default" + std::to_string(i);
        std::copy(label.begin(), label.end(), pool[i].begin());
        pool[i][label.size()] = 0;
        std::memcpy(&ram[0x121292 + i * 20], pool[i].data(), 20);
    }
    for (unsigned i = 0; i < 10; ++i) {
        scores[i] = 10000 - static_cast<std::int32_t>(i) * 1000;
        std::memcpy(&ram[0x1211ad + i * 25], &scores[i], 4);
    }
}

static void check_defaults(void)
{
    reset();
    RandomNames pool{};
    DefaultScores scores{};
    set_defaults_source(pool, scores);
    State reference{};
    reference.records[0].name.fill(0xe7);
    std::memset(g_hiscore_table, 0xe7, sizeof(g_hiscore_table));
    // Each random value is a full 16-bit unsigned result, as observed at 4077.
    for (unsigned i = 0; i < 20; ++i)
        numbers.push_back(static_cast<std::uint16_t>(i * 3187u + 0xff01u));
    std::size_t next_ref = 0;
    assert(initialize_defaults(reference, pool, scores,
                               [&] { return numbers[next_ref++]; }));
    // Preserve all per-record padding from the same initial table state.
    for (unsigned i = 0; i < 10; ++i)
        reference.records[i].name.fill(0xe7);
    next_ref = 0;
    assert(initialize_defaults(reference, pool, scores,
                               [&] { return numbers[next_ref++]; }));
    hiscore_generate_defaults_4050();
    check_state(reference);
    assert(next_number == 20 && seeds == 1);
}

static void check_insert(void)
{
    reset();
    std::mt19937 rng(0xef2000);
    for (unsigned trial = 0; trial < 2000; ++trial) {
        State original = random_state(rng);
        auto current = static_cast<std::int32_t>(rng());
        set_current_score(original, current);
        // Signed order; add equal-score/INT_MIN cases throughout the run.
        std::array<std::int32_t, 10> scores{};
        for (auto& n : scores) n = static_cast<std::int32_t>(rng());
        std::sort(scores.begin(), scores.end(), std::greater<std::int32_t>());
        for (unsigned i = 0; i < 10; ++i) set_score(original.records[i], scores[i]);
        if (trial % 13 == 0) set_current_score(original, scores[trial % 10]);
        if (trial % 19 == 0) set_score(original.records[9], INT32_MIN);
        Name input{};
        input.fill(static_cast<std::uint8_t>(rng()));
        std::string label = "Ace" + std::to_string(trial);
        std::copy(label.begin(), label.end(), input.begin());
        input[label.size()] = 0;
        std::memcpy(&ram[0x7000], input.data(), 20);
        auto difficulty = static_cast<std::uint8_t>(trial % 3 + 1);
        ram[0x121e44] = difficulty;
        State reference = original;
        auto expected_slot = insert(reference, input, difficulty);
        set_state(original);
        assert(hiscore_insert_433c(0x7000) == expected_slot);
        check_state(reference);
    }
}

static void check_files(void)
{
    reset();
    std::mt19937 rng(0x533f);
    State original = random_state(rng);
    set_state(original);
    assert(hiscore_save_42a4() == 1);
    assert(file_data.size() == kFileBytes);
    auto reference_bytes = encode_file(original);
    assert(std::equal(file_data.begin(), file_data.end(), reference_bytes.begin()));
    assert(file_closes == 1);
    check_state(original);

    // The checksum deliberately ignores corruption of all but the first byte.
    file_data[100] ^= 0xff;
    State loaded = original;
    assert(decode_file(loaded, file_data.data(), file_data.size()));
    g_hiscore_current = 0x12345678;
    assert(hiscore_load_41fc() == 1);
    assert(g_hiscore_current == 0x12345678);
    assert(std::memcmp(g_hiscore_table, loaded.records.data(), kTableBytes) == 0);
    assert(file_closes == 2 && file_opens == 2);
    assert(ui_refreshes == 1 && icon_starts == 3);

    // A changed first name byte invalidates the checksum, but a successful
    // read still returns 1 and refreshes after regenerating defaults.
    reset();
    original = random_state(rng);
    set_state(original);
    file_data.assign(reference_bytes.begin(), reference_bytes.end());
    file_data[4] ^= 1;
    RandomNames corrupt_pool{};
    DefaultScores corrupt_scores{};
    set_defaults_source(corrupt_pool, corrupt_scores);
    for (unsigned i = 0; i < 20; ++i)
        numbers.push_back(static_cast<std::uint16_t>(i * 573u));
    State corrupt_fallback = original;
    assert(!decode_file(corrupt_fallback, file_data.data(), file_data.size()));
    std::size_t corrupt_next = 0;
    assert(initialize_defaults(corrupt_fallback, corrupt_pool, corrupt_scores,
                               [&] { return numbers[corrupt_next++]; }));
    assert(hiscore_load_41fc() == 1);
    check_state(corrupt_fallback);
    assert(seeds == 1 && file_opens == 1 && file_closes == 1);
    assert(ui_refreshes == 1 && icon_starts == 3);

    reset();
    original = random_state(rng);
    set_state(original);
    file_exists = false;
    RandomNames pool{};
    DefaultScores scores{};
    set_defaults_source(pool, scores);
    for (unsigned i = 0; i < 20; ++i) numbers.push_back(static_cast<std::uint16_t>(i * 77));
    State fallback = original;
    auto ref_index = std::size_t{0};
    assert(initialize_defaults(fallback, pool, scores, [&] { return numbers[ref_index++]; }));
    assert(hiscore_load_41fc() == 0);
    check_state(fallback);
    assert(seeds == 1 && file_opens == 0 && ui_refreshes == 0);

    reset();
    set_state(original);
    open_succeeds = false;
    set_defaults_source(pool, scores);
    for (unsigned i = 0; i < 20; ++i)
        numbers.push_back(static_cast<std::uint16_t>(i * 77));
    assert(hiscore_load_41fc() == 0);
    check_state(fallback);
    assert(seeds == 1 && file_opens == 1 && file_closes == 0);
    assert(ui_refreshes == 0);

    reset();
    set_state(original);
    open_succeeds = false;
    assert(hiscore_save_42a4() == 0);
    check_state(original);
    assert(file_data.empty() && file_closes == 0);
}

static void check_display(void)
{
    reset();
    std::mt19937 rng(25);
    auto state = random_state(rng);
    for (unsigned i = 0; i < 10; ++i) {
        set_score(state.records[i], static_cast<std::int32_t>(i) * -100);
        state.records[i].difficulty = static_cast<std::uint8_t>(i % 3 + 1);
    }
    state.records[2].difficulty = 9;
    set_state(state);
    icon_counts = {4, 5, 11};
    for (unsigned c = 0; c < 3; ++c) {
        for (unsigned i = 0; i < icon_counts[c]; ++i) {
            const std::uint32_t icon = 0x300000u + 0x1000u * c + 0x40u * i;
            const std::uint32_t next = i + 1 == icon_counts[c] ? 0 : icon + 0x40u;
            std::memcpy(&ram[icon + 0x21u], &next, 4);
        }
    }
    window_exists = false;
    hiscore_refresh_display_40dc(0);
    assert(ui_events.empty() && icon_starts == 0);
    hiscore_refresh_display_40dc(1);
    assert(icon_starts == 3 && ui_events.size() == 19);
    assert(ui_events[0] == std::to_string(0x300000u) + ":Pilot0");
    assert(ui_events[4] == std::to_string(0x301000u) + ":Rookie");
    assert(ui_events[6] == "invalid");
    assert(ui_events[9] == std::to_string(0x302000u) + ":p0");
    assert(ui_events[11] == std::to_string(0x302080u) + ":p-200");
    assert(ui_events.back() == std::to_string(0x302240u) + ":p-900");
}

int main()
{
    check_defaults();
    check_insert();
    check_files();
    check_display();
    return 0;
}
