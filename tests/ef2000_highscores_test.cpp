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

class RecordingDisplay final : public DisplaySink {
public:
    struct Event {
        DisplayColumn column;
        std::size_t index;
        std::string text;
    };

    std::array<std::size_t, 3> lengths{};
    std::vector<Event> events;
    std::vector<std::pair<std::size_t, std::uint8_t>> errors;

    std::size_t length(DisplayColumn column) const override {
        return lengths[static_cast<std::size_t>(column)];
    }
    void set_text(DisplayColumn column, std::size_t index,
                  std::string_view text) override {
        events.push_back({column, index, std::string(text)});
    }
    void invalid_difficulty(std::size_t index, std::uint8_t value) override {
        errors.emplace_back(index, value);
    }
};

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

void test_display_three_independent_linked_columns() {
    const auto table = example_table();
    RecordingDisplay sink;
    sink.lengths = {12, 4, 2};
    auto result = refresh_display(table, sink, true, false);
    assert(result.status == DisplayStatus::Updated);
    assert((result.changed == std::array<std::size_t, 3>{10, 4, 2}));
    assert(result.invalid_names == 0 && result.invalid_difficulties == 0);
    assert(sink.events.size() == 16);
    for (std::size_t i = 0; i < 10; ++i) {
        assert(sink.events[i].column == DisplayColumn::Names);
        assert(sink.events[i].index == i);
        assert(sink.events[i].text == "Player" + std::to_string(i));
    }
    for (std::size_t i = 0; i < 4; ++i) {
        assert(sink.events[10 + i].column == DisplayColumn::Difficulty);
        assert(sink.events[10 + i].index == i);
    }
    assert(sink.events[10].text == "Rookie");
    assert(sink.events[11].text == "Profi");
    assert(sink.events[12].text == "Topgun");
    assert(sink.events[14].column == DisplayColumn::Score);
    assert(sink.events[14].text == "p1000");
    assert(sink.events[15].text == "p900");
}

void test_hidden_window_force_and_bad_guest_data() {
    State table = example_table();
    RecordingDisplay sink;
    sink.lengths = {10, 10, 10};
    const auto hidden = refresh_display(table, sink, false, false);
    assert(hidden.status == DisplayStatus::Hidden);
    assert(sink.events.empty());
    const auto missing_forced = refresh_display(table, sink, false, true);
    assert(missing_forced.status == DisplayStatus::MissingForcedWindow);
    assert(sink.events.empty());

    table.records[0].name.fill('Q'); // guest would read past its name slot
    table.records[1].difficulty = 0; // original invalid-difficulty callback
    table.records[4].difficulty = 4;
    sink.lengths = {2, 5, 1};
    const auto old_bytes = encode_file(table);
    const auto result = refresh_display(table, sink, true, true);
    assert(result.status == DisplayStatus::Updated);
    assert(result.invalid_names == 1);
    assert(result.invalid_difficulties == 2);
    assert((result.changed == std::array<std::size_t, 3>{1, 3, 1}));
    assert(sink.events.size() == 5);
    assert(sink.events[0].text == "Player1");
    assert(sink.errors.size() == 2);
    assert(sink.errors[0] == std::make_pair(std::size_t{1}, std::uint8_t{0}));
    assert(sink.errors[1] == std::make_pair(std::size_t{4}, std::uint8_t{4}));
    assert(encode_file(table) == old_bytes); // display code never edits guest table
}

void test_arcade_name_acceptance_and_empty_fallback() {
    State table = example_table();
    table.records[2].name[15] = 0x5a;
    assert(accept_arcade_name(table, 2, name_of("New Pilot")) == RenameResult::Updated);
    assert(name_text(table.records[2].name) == "New Pilot");
    assert(table.records[2].name[15] == 0x5a); // guest strcpy leaves padding

    assert(accept_arcade_name(table, 2, name_of("")) == RenameResult::Updated);
    assert(name_text(table.records[2].name) == "Jemand");
    assert(table.records[2].name[15] == 0x5a);
    assert(accept_arcade_name(table, 2, name_of(" ")) == RenameResult::Updated);
    assert(name_text(table.records[2].name) == " "); // strlen, no whitespace trim

    const auto before = encode_file(table);
    assert(accept_arcade_name(table, -1, name_of("Ignore")) == RenameResult::InvalidSlot);
    assert(accept_arcade_name(table, 10, name_of("Ignore")) == RenameResult::InvalidSlot);
    Name invalid{};
    invalid.fill('X');
    assert(accept_arcade_name(table, 2, invalid) == RenameResult::UnterminatedName);
    assert(encode_file(table) == before);
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
    test_display_three_independent_linked_columns();
    test_hidden_window_force_and_bad_guest_data();
    test_arcade_name_acceptance_and_empty_fallback();
    std::cout << "ef2000-highscores: layout, insertion, defaults and guest checksum passed\n";
}
