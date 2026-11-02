// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A typed, host-side reconstruction of the original EF2000 high-score core.
// This is not yet connected to a game runtime. All original assets stay external.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace ef2000::highscores {

constexpr std::uint32_t kGuestCurrentScore = 0x1970a0;
constexpr std::uint32_t kGuestRecords = 0x1970a4;
constexpr std::size_t kNameBytes = 20;
constexpr std::size_t kRecordBytes = 25;
constexpr std::size_t kRecordCount = 10;
constexpr std::size_t kRandomNameCount = 48;
constexpr std::size_t kTableBytes = kRecordBytes * kRecordCount;
constexpr std::size_t kFileBytes = 4 + kTableBytes;

using Name = std::array<std::uint8_t, kNameBytes>;
using RandomNames = std::array<Name, kRandomNameCount>;
using DefaultScores = std::array<std::int32_t, kRecordCount>;
using Random16 = std::function<std::uint16_t()>;

// Wire layout verified in the original guest instructions at 0x4050, 0x433c,
// 0x41fc and 0x42a4. Byte arrays avoid host structure padding and unaligned
// int32_t accesses (score lies at record offset 21).
struct Record {
    Name name{};
    std::uint8_t difficulty = 0;
    std::array<std::uint8_t, 4> score_le{};
};

struct State {
    std::array<std::uint8_t, 4> current_score_le{};
    std::array<Record, kRecordCount> records{};
};

static_assert(sizeof(Record) == kRecordBytes);
static_assert(sizeof(State) == 4 + kTableBytes);
static_assert(offsetof(Record, difficulty) == 20);
static_assert(offsetof(Record, score_le) == 21);
static_assert(offsetof(State, records) == 4);

std::int32_t score(const Record& record);
void set_score(Record& record, std::int32_t value);
std::int32_t current_score(const State& state);
void set_current_score(State& state, std::int32_t value);

// 0x432c uses a *strict* greater-than comparison against the tenth slot.
bool exceeds_last_score(const State& state);

// 0x433c inserts a name using the original unbounded strcpy semantics *for
// valid 20-byte NUL-terminated names*: only bytes through the first NUL are
// copied, leaving destination name padding untouched. Ties are inserted before
// existing equal scores. Returns 0..9, -1 if no slot qualifies, or host-only
// -2 for invalid unterminated input/shift records (avoids a guest overrun).
int insert(State& state, const Name& name, std::uint8_t difficulty);

// 0x4050 selects ten names from a 48-entry, 20-byte name pool and uses the
// original default-score template entries. Random16 supplies successive raw
// unsigned 16-bit values from the game's RNG; this module does not model RNG
// seeding. Returns false if a pool name is unterminated; no writes in that case.
bool initialize_defaults(State& state, const RandomNames& pool,
                         const DefaultScores& template_scores,
                         const Random16& next_random);

// 0x41fc / 0x42a4 store a 32-bit LE "checksum" followed by the 250 table
// bytes. ORIGINAL BUG: the loop never advances its byte pointer; it sums
// the first byte 250 times, not the entire table. Both functions do this.
std::uint32_t original_checksum(const State& state);
std::array<std::uint8_t, kFileBytes> encode_file(const State& state);

// Copies a complete on-disk table into state and returns the original
// checksum comparison. A mismatched checksum still leaves the copied data in
// state, as 0x41fc does before it calls default initialization. A truncated
// buffer is rejected without mutation (unlike the DOS read API's unchecked
// short reads, which need separate emulation if required).
bool decode_file(State& state, const std::uint8_t* data, std::size_t size);

// 0x40dc updates three GUI icon chains under the "hiscores" window. Actual
// GUI objects use a guest-specific +0x21 next pointer and remain outside this
// model. A host view implements independent chain lengths and text updates.
enum class DisplayColumn { Names, Difficulty, Score };
enum class DisplayStatus { Updated, Hidden, MissingForcedWindow };

class DisplaySink {
public:
    virtual ~DisplaySink() = default;
    virtual std::size_t length(DisplayColumn column) const = 0;
    // The supplied view is valid for this call only; a GUI must copy it.
    virtual void set_text(DisplayColumn column, std::size_t index,
                          std::string_view text) = 0;
    virtual void invalid_difficulty(std::size_t index, std::uint8_t value) = 0;
};

struct DisplayResult {
    DisplayStatus status = DisplayStatus::Updated;
    std::array<std::size_t, 3> changed{};
    std::size_t invalid_names = 0;
    std::size_t invalid_difficulties = 0;
};

// 0x40dc skips the work if the highscores window isn't present and EAX is
// zero. A nonzero EAX bypasses that initial check; missing forced windows
// reach a guest GUI error, which the host model reports without dereferencing.
// Original bad-difficulty logging may itself be fatal; here it is a callback.
DisplayResult refresh_display(const State& state, DisplaySink& sink,
                              bool window_present, bool force);

// In the callback named InitArcade at 0x4c50, the selected slot's name is
// copied from the GUI edit field, then checked by strlen at 0xb1b8b.
// Empty input is replaced by the literal "Jemand" at 0x130be8. Save/refresh
// and menu-close effects happen afterwards and are not part of this helper.
enum class RenameResult { Updated, InvalidSlot, UnterminatedName };
RenameResult accept_arcade_name(State& state, int selected_slot, const Name& edit_text);

} // namespace ef2000::highscores
