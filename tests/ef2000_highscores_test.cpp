// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../runtime/ef2000_highscores.hpp"

#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG // Keep assertions live when CMake builds tests in Release mode.
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace ef2000::highscores;

namespace {

Name name_of(const std::string& text) {
    assert(text.size() < kNameBytes);
    Name name{};
    std::copy(text.begin(), text.end(), name.begin());
    return name;
}

std::string name_text(const Name& name) {
    const auto end = std::find(name.begin(), name.end(), std::uint8_t{0});
    return std::string(name.begin(), end);
}

State example_table() {
    State table{};
    for (std::size_t slot = 0; slot < kRecordCount; ++slot) {
        table.records[slot].name = name_of("Player" + std::to_string(slot));
        table.records[slot].difficulty = static_cast<std::uint8_t>(slot % 3 + 1);
        set_score(table.records[slot], static_cast<int>(1000 - slot * 100));
    }
    return table;
}

void test_wire_layout_and_signed_scores() {
    static_assert(sizeof(Record) == 25);
    static_assert(sizeof(State) == 254);
    State table{};
    set_current_score(table, -123456);
    assert(current_score(table) == -123456);
    assert(table.current_score_le[0] == 0xc0);
    set_score(table.records[3], -1);
    assert(score(table.records[3]) == -1);
    assert(table.records[3].score_le[0] == 0xff);
    assert(table.records[3].score_le[3] == 0xff);
}

void test_insert_and_preserve_unused_name_padding() {
    State table = example_table();
    // The original guest strcpy never zeroes the destination's name padding.
    table.records[2].name[10] = 0x77;
    table.records[3].name[10] = 0x99;
    set_current_score(table, 850);
    assert(insert(table, name_of("Ace"), 3) == 2);
    assert(name_text(table.records[2].name) == "Ace");
    assert(score(table.records[2]) == 850);
    assert(table.records[2].difficulty == 3);
    assert(table.records[3].name[10] == 0x99); // not source slot 2's 0x77
    assert(name_text(table.records[3].name) == "Player2");
    assert(table.records[3].difficulty == 3);
    assert(score(table.records[3]) == 800);
    assert(name_text(table.records[9].name) == "Player8");
    assert(current_score(table) == 850);
}

void test_ties_and_two_distinct_comparisons() {
    State table = example_table();
    set_current_score(table, 100);
    assert(!exceeds_last_score(table)); // 0x432c: strict greater-than
    assert(insert(table, name_of("Equal"), 1) == 9); // 0x433c: tie qualifies
    assert(name_text(table.records.back().name) == "Equal");

    table = example_table();
    set_current_score(table, 99);
    const State before = table;
    assert(insert(table, name_of("TooLow"), 2) == -1);
    assert(encode_file(table) == encode_file(before));
    assert(current_score(table) == current_score(before));

    // DEC on stored INT_MIN wraps to INT_MAX. This is a guest-specific edge
    // where the insertion check disagrees with ordinary signed >= comparison.
    for (auto& record : table.records) {
        set_score(record, std::numeric_limits<std::int32_t>::min());
    }
    set_current_score(table, 0);
    assert(exceeds_last_score(table));
    assert(insert(table, name_of("Wrap"), 1) == -1);
}

void test_every_insertion_rank() {
    for (int position = 0; position < static_cast<int>(kRecordCount); ++position) {
        State table = example_table();
        const int incoming = 1000 - position * 100 + 1;
        set_current_score(table, incoming);
        assert(insert(table, name_of("New"), 2) == position);
        for (int slot = 0; slot < static_cast<int>(kRecordCount); ++slot) {
            const int expected = slot < position ? 1000 - slot * 100 :
                                 slot == position ? incoming : 1100 - slot * 100;
            assert(score(table.records[slot]) == expected);
            if (slot > 0) {
                assert(score(table.records[slot - 1]) >= score(table.records[slot]));
            }
        }
        // Equal scores must also be placed before the first matching record.
        table = example_table();
        set_current_score(table, 1000 - position * 100);
        assert(insert(table, name_of("Tie"), 1) == position);
        assert(name_text(table.records[position].name) == "Tie");
    }
}

void test_host_guards_reject_unsafe_guest_strings() {
    State table = example_table();
    set_current_score(table, 750);
    const auto before = encode_file(table);
    Name unterminated{};
    unterminated.fill('Q');
    assert(insert(table, unterminated, 2) == -2);
    assert(encode_file(table) == before);
    table.records[4].name = unterminated;
    const auto before_bad_shift = encode_file(table);
    assert(insert(table, name_of("Good"), 2) == -2);
    assert(encode_file(table) == before_bad_shift);
}

void test_default_names_difficulty_and_template_scores() {
    State table{};
    set_current_score(table, 777);
    RandomNames pool{};
    DefaultScores sources{};
    for (std::size_t i = 0; i < pool.size(); ++i) {
        pool[i] = name_of("Name" + std::to_string(i));
    }
    for (std::size_t i = 0; i < sources.size(); ++i) {
        sources[i] = static_cast<int>(10000 - i * 1000);
    }
    const std::vector<std::uint16_t> random_values = {
        47, 2, 48, 0, 1, 1, 2, 2, 3, 3,
        4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    };
    std::size_t consumed = 0;
    const auto random = [&]() { return random_values.at(consumed++); };
    assert(initialize_defaults(table, pool, sources, random));
    assert(consumed == 20);
    assert(name_text(table.records[0].name) == "Name47");
    assert(table.records[0].difficulty == 3);
    assert(name_text(table.records[1].name) == "Name0");
    assert(table.records[1].difficulty == 1);
    assert(table.records[2].difficulty == 2);
    assert(score(table.records[0]) == 10000);
    assert(score(table.records[9]) == 1000);
    assert(current_score(table) == 777); // initialization leaves score global alone

    pool[13].fill('X');
    const auto before = encode_file(table);
    assert(!initialize_defaults(table, pool, sources, random));
    assert(encode_file(table) == before);
    assert(consumed == 20);
}

void test_guest_file_checksum_is_only_first_byte() {
    State table = example_table();
    table.records[0].name = name_of("A");
    const auto file = encode_file(table);
    assert(file.size() == 254);
    assert(file[0] == 0x7a); // 'A' * 250 = 16250 = 0x3f7a
    assert(file[1] == 0x3f);
    assert(file[2] == 0 && file[3] == 0 && file[4] == 'A');
    assert(original_checksum(table) == 16250);

    State loaded{};
    set_current_score(loaded, -999);
    assert(decode_file(loaded, file.data(), file.size()));
    assert(current_score(loaded) == -999); // file omits current score global
    assert(encode_file(loaded) == file);

    // Original code never increments EBX at 0x426d..0x427a or 0x42d4..0x42e1.
    // Changing any other byte does not invalidate the alleged checksum.
    auto modified = file;
    modified.back() ^= 0x80;
    assert(decode_file(loaded, modified.data(), modified.size()));
    assert(encode_file(loaded) == modified);

    modified = file;
    modified[4] = 'B';
    assert(!decode_file(loaded, modified.data(), modified.size()));
    assert(loaded.records[0].name[0] == 'B'); // checksum is checked after copy
    const auto after_bad_checksum = encode_file(loaded);
    assert(!decode_file(loaded, file.data(), file.size() - 1));
    assert(encode_file(loaded) == after_bad_checksum);
    assert(!decode_file(loaded, nullptr, file.size()));
}

} // namespace

int main() {
    test_wire_layout_and_signed_scores();
    test_insert_and_preserve_unused_name_padding();
    test_ties_and_two_distinct_comparisons();
    test_every_insertion_rank();
    test_host_guards_reject_unsafe_guest_strings();
    test_default_names_difficulty_and_template_scores();
    test_guest_file_checksum_is_only_first_byte();
    std::cout << "ef2000-highscores: layout, insertion, defaults and guest checksum passed\n";
}
