/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

/*
 * The AIL XMIDI timer service.  Each tick advances every playing sequence
 * of a MIDI driver: it times out held notes, interprets the event stream
 * (delays, channel events, sysex and the end-of-track, tempo and
 * time-signature meta events), counts beats and ramps the volume and the
 * tempo towards their targets.
 *
 * The original keeps its loop state in globals; they are written here in
 * the same way because the callbacks may look at them.
 */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* timer record fields */
#define TIMER_LOCKED      0x14u
#define TIMER_SEQUENCES   0x18u   /* array of SEQ_SIZE records */
#define TIMER_COUNT       0x1cu

/* sequence record fields */
#define SEQ_SIZE          0x718u
#define SEQ_DRIVER        0x000u
#define SEQ_STATUS        0x004u  /* 4 = playing, 2 = done */
#define SEQ_EVENTS        0x010u  /* event chunk; the stream starts at +8 */
#define SEQ_CURSOR        0x014u
#define SEQ_BEAT_CALLBACK 0x024u
#define SEQ_END_CALLBACK  0x028u
#define SEQ_LOOPS         0x02cu  /* 0 = forever */
#define SEQ_DELAY         0x030u
#define SEQ_TICKS         0x034u
#define SEQ_VOLUME        0x038u
#define SEQ_VOLUME_TARGET 0x03cu
#define SEQ_VOLUME_ACCUM  0x040u
#define SEQ_VOLUME_PERIOD 0x044u
#define SEQ_TEMPO         0x048u  /* percent */
#define SEQ_TEMPO_TARGET  0x04cu
#define SEQ_TEMPO_ACCUM   0x050u
#define SEQ_TEMPO_PERIOD  0x054u
#define SEQ_TEMPO_BALANCE 0x058u
#define SEQ_BEAT          0x05cu
#define SEQ_MEASURE       0x060u
#define SEQ_BEATS_PER_BAR 0x064u
#define SEQ_BEAT_STEP     0x068u
#define SEQ_BEAT_FRACTION 0x06cu
#define SEQ_BEAT_LENGTH   0x070u
#define SEQ_ACTIVE_NOTES  0x554u
#define SEQ_NOTE_CHANNEL  0x558u  /* [32], -1 = free */
#define SEQ_NOTE_KEY      0x5d8u  /* [32] */
#define SEQ_NOTE_TIME     0x658u  /* [32] remaining ticks */
#define NOTE_SLOTS        0x20u

#define DRIVER_RATE       0x10u

/* the service loop state */
#define g_service_busy    EF_U32(0x0017e258u)
#define g_sequences_left  EF_U32(0x004ddcd8u)
#define g_sequence        EF_U32(0x004ddccc)
#define g_note_slot_addr  0x004ddcd0u
#define g_note_slot       EF_U32(g_note_slot_addr)
#define g_beat_info_addr  0x004ddcd4u
#define g_beat_info       EF_U32(g_beat_info_addr)
#define g_stream_ended    EF_U32(0x004ddcdcu)
#define g_status          EF_U32(0x004ddcc0u)
#define g_channel         EF_U32(0x004ddcb4u)
#define g_meta_type       EF_U32(0x004ddcc8u)
#define g_event_length    EF_U32(0x004ddcc4u)
#define g_shift           EF_U32(0x004ddcbcu)
#define g_quarter_rate    EF_U32(0x004ddcb8u)
#define g_sysex_cursor_addr 0x004ddcb0u
#define g_sysex_cursor    EF_U32(g_sysex_cursor_addr)
#define g_service_rate    EF_S32(0x004dd7f8u)

#define SEQ(field)        EF_U32(g_sequence + (field))
#define SEQ_S(field)      EF_S32(g_sequence + (field))
#define NOTE(field, n)    EF_U32(g_sequence + (n) * 4u + (field))

#define AIL_ERROR_TEXT    0x004dd6b0u
#define NOTE_OVERFLOW_TEXT 0x001953d0u  /* "Internal note queue overflow\n" */
#define NOTE_OVERFLOW_SIZE 30u

static void release_expired_notes(void)
{
	if (SEQ_S(SEQ_ACTIVE_NOTES) <= 0)
		return;
	for (g_note_slot = 0; g_note_slot < NOTE_SLOTS; g_note_slot++) {
		if (NOTE(SEQ_NOTE_CHANNEL, g_note_slot) == 0xffffffffu)
			continue;
		NOTE(SEQ_NOTE_TIME, g_note_slot) -= 1;
		if (EF_S32(g_sequence + g_note_slot * 4u + SEQ_NOTE_TIME) > 0)
			continue;
		guest_fn_1123a4_x(g_sequence,
				NOTE(SEQ_NOTE_CHANNEL, g_note_slot) | 0x80u,
				NOTE(SEQ_NOTE_KEY, g_note_slot), 0, 0);
		NOTE(SEQ_NOTE_CHANNEL, g_note_slot) = 0xffffffffu;
		SEQ(SEQ_ACTIVE_NOTES) -= 1;
		if (SEQ(SEQ_ACTIVE_NOTES) == 0)
			return;
	}
}

static void call_beat_callback(uint32_t beat, uint32_t measure)
{
	guest_callptr4c_x(SEQ(SEQ_BEAT_CALLBACK), SEQ(SEQ_DRIVER), g_sequence,
			beat, measure);
}

/* End of track: loop back to the start, or finish the sequence. */
static void end_of_track(void)
{
	g_stream_ended = 1;
	if (SEQ(SEQ_LOOPS) != 0) {
		SEQ(SEQ_LOOPS) -= 1;
		if (SEQ(SEQ_LOOPS) == 0) {
			trace_call_103148(g_sequence);
			SEQ(SEQ_STATUS) = 2;
			if (SEQ(SEQ_END_CALLBACK) != 0)
				guest_callptr1_x(SEQ(SEQ_END_CALLBACK), g_sequence);
			return;
		}
	}
	SEQ(SEQ_MEASURE) = 0xffffffffu;
	SEQ(SEQ_BEAT) = 0;
	SEQ(SEQ_BEAT_FRACTION) = 0;
	SEQ(SEQ_CURSOR) = SEQ(SEQ_EVENTS) + 8u;
	if (SEQ(SEQ_BEAT_CALLBACK) != 0)
		call_beat_callback(0, 0);
}

static void set_tempo(void)
{
	ef_addr32 data = SEQ(SEQ_CURSOR);

	g_shift = ((uint32_t)EF_U8(data) << 16) + ((uint32_t)EF_U8(data + 1) << 8) +
		EF_U8(data + 2);
	SEQ(SEQ_BEAT_LENGTH) = g_shift << 4;
}

static void set_time_signature(void)
{
	ef_addr32 data = SEQ(SEQ_CURSOR);
	int32_t step;

	SEQ(SEQ_BEATS_PER_BAR) = EF_U8(data);
	g_shift = (uint32_t)EF_U8(data + 1) - 2u;
	step = (int32_t)0xf42400 / g_service_rate;
	g_quarter_rate = (uint32_t)step;
	if ((int32_t)g_shift < 0) {
		g_shift = 0u - g_shift;
		step >>= (uint8_t)g_shift;
	} else {
		step <<= (uint8_t)g_shift;
	}
	SEQ(SEQ_BEAT_STEP) = (uint32_t)step;
	SEQ(SEQ_BEAT_FRACTION) = 0;
	SEQ(SEQ_BEAT) = 0;
	SEQ(SEQ_BEAT_FRACTION) = 0u - SEQ(SEQ_BEAT_STEP);
	SEQ(SEQ_MEASURE) += 1;
	if (SEQ(SEQ_BEAT_CALLBACK) != 0)
		call_beat_callback(SEQ(SEQ_BEAT), SEQ(SEQ_MEASURE));
}

static void meta_event(void)
{
	ef_addr32 data = SEQ(SEQ_CURSOR);

	g_meta_type = EF_U8(data + 1);
	SEQ(SEQ_CURSOR) = data + 2u;
	g_event_length = read_varlen_111f20(0, 0, 0, 0, g_sequence + SEQ_CURSOR);
	if (g_meta_type == 0x2f)
		end_of_track();
	else if (g_meta_type == 0x51)
		set_tempo();
	else if (g_meta_type == 0x58)
		set_time_signature();
	SEQ(SEQ_CURSOR) += g_event_length;
}

static void sysex_event(ef_addr32 timer)
{
	uint32_t length;

	g_sysex_cursor = SEQ(SEQ_CURSOR) + 1u;
	length = read_varlen_111f20(0, 0, 0, 0, g_sysex_cursor_addr);
	g_event_length = length + (g_sysex_cursor - SEQ(SEQ_CURSOR));
	append_midi_block_111ed4(0, 0, 0, 0, timer, SEQ(SEQ_CURSOR), g_event_length);
	SEQ(SEQ_CURSOR) += g_event_length;
}

/* A channel event; a note-on also takes a note slot and its duration.
 * Returns 0 when the note queue overflowed. */
static int channel_event(void)
{
	ef_addr32 data;
	ef_addr32 seq;
	ef_addr32 src;
	ef_addr32 dst;
	uint32_t i;

	g_channel = g_status & 0xfu;
	g_status &= 0xf0u;
	data = SEQ(SEQ_CURSOR);
	guest_fn_1123a4_x(g_sequence, EF_U8(data), EF_U8(data + 1),
			EF_U8(data + 2), 1);
	if (g_status != 0x90u) {
		seq = g_sequence;
		EF_U32(seq + SEQ_CURSOR) += midi_data_length_111d98(
				EF_U8(EF_U32(seq + SEQ_CURSOR)));
		return 1;
	}

	seq = g_sequence;
	for (g_note_slot = 0; g_note_slot < NOTE_SLOTS; g_note_slot++)
		if (EF_U32(seq + g_note_slot * 4u + SEQ_NOTE_CHANNEL) == 0xffffffffu)
			break;
	if (g_note_slot == NOTE_SLOTS) {
		src = NOTE_OVERFLOW_TEXT;
		dst = AIL_ERROR_TEXT;
		for (i = 0; i < NOTE_OVERFLOW_SIZE; i++)
			EF_U8(dst + i) = EF_U8(src + i);
		trace_call_103148(g_sequence);
		return 0;
	}
	SEQ(SEQ_ACTIVE_NOTES) += 1;
	NOTE(SEQ_NOTE_CHANNEL, g_note_slot) = g_channel;
	NOTE(SEQ_NOTE_KEY, g_note_slot) = EF_U8(SEQ(SEQ_CURSOR) + 1);
	SEQ(SEQ_CURSOR) += 3u;
	NOTE(SEQ_NOTE_TIME, g_note_slot) =
		read_varlen_111f20(0, 0, 0, 0, g_sequence + SEQ_CURSOR);
	return 1;
}

/* Interprets events until the next delay.  Returns 0 when the note queue
 * overflowed. */
static int run_events(ef_addr32 timer)
{
	for (;;) {
		g_status = EF_U8(SEQ(SEQ_CURSOR));
		if (g_status < 0x80u || g_stream_ended != 0)
			break;
		if (g_status == 0xffu)
			meta_event();
		else if (g_status == 0xf0u || g_status == 0xf7u)
			sysex_event(timer);
		else if (!channel_event())
			return 0;
	}
	if (g_stream_ended == 0) {
		SEQ(SEQ_DELAY) = EF_U8(SEQ(SEQ_CURSOR));
		SEQ(SEQ_CURSOR) += 1;
	}
	return 1;
}

static void count_beat(void)
{
	SEQ(SEQ_BEAT_FRACTION) += SEQ(SEQ_BEAT_STEP);
	if (SEQ_S(SEQ_BEAT_FRACTION) < SEQ_S(SEQ_BEAT_LENGTH))
		return;
	SEQ(SEQ_BEAT_FRACTION) -= SEQ(SEQ_BEAT_LENGTH);
	SEQ(SEQ_BEAT) += 1;
	if (SEQ_S(SEQ_BEAT) >= SEQ_S(SEQ_BEATS_PER_BAR)) {
		SEQ(SEQ_BEAT) = 0;
		SEQ(SEQ_MEASURE) += 1;
	}
	if (SEQ(SEQ_BEAT_CALLBACK) != 0) {
		trace_call_103778(0, 0, 0, 0, g_sequence, g_note_slot_addr,
				g_beat_info_addr);
		call_beat_callback(g_note_slot, g_beat_info);
	}
}

/* Moves *value one step towards *target per elapsed period. */
static void ramp(uint32_t value, uint32_t target, uint32_t accum,
		uint32_t period)
{
	while (SEQ_S(accum) >= SEQ_S(period)) {
		SEQ(accum) -= SEQ(period);
		if (SEQ_S(target) > SEQ_S(value))
			SEQ(value) += 1;
		else
			SEQ(value) -= 1;
		if (SEQ(value) == SEQ(target))
			break;
	}
}

static void ramp_volume_and_tempo(void)
{
	if (SEQ(SEQ_VOLUME) != SEQ(SEQ_VOLUME_TARGET)) {
		SEQ(SEQ_VOLUME_ACCUM) += EF_U32(SEQ(SEQ_DRIVER) + DRIVER_RATE);
		ramp(SEQ_VOLUME, SEQ_VOLUME_TARGET, SEQ_VOLUME_ACCUM, SEQ_VOLUME_PERIOD);
		if ((EF_U8(g_sequence + SEQ_TICKS) & 7u) == 0)
			send_channel_volumes_112bd8(0, 0, 0, 0, g_sequence);
	}
	if (SEQ(SEQ_TEMPO) != SEQ(SEQ_TEMPO_TARGET)) {
		SEQ(SEQ_TEMPO_ACCUM) += EF_U32(SEQ(SEQ_DRIVER) + DRIVER_RATE);
		ramp(SEQ_TEMPO, SEQ_TEMPO_TARGET, SEQ_TEMPO_ACCUM, SEQ_TEMPO_PERIOD);
	}
}

/* Advances one sequence.  Returns 0 when the note queue overflowed. */
static int serve_sequence(ef_addr32 timer)
{
	SEQ(SEQ_TICKS) += 1;
	g_stream_ended = 0;
	SEQ(SEQ_TEMPO_BALANCE) += SEQ(SEQ_TEMPO);
	while (SEQ_S(SEQ_TEMPO_BALANCE) >= 100) {
		SEQ(SEQ_TEMPO_BALANCE) -= 100;
		release_expired_notes();
		SEQ(SEQ_DELAY) -= 1;
		if (SEQ_S(SEQ_DELAY) <= 0 && !run_events(timer))
			return 0;
		if (g_stream_ended == 0)
			count_beat();
	}
	if (g_stream_ended == 0)
		ramp_volume_and_tempo();
	return 1;
}

/* 0x112c10, 1704 bytes. */
void EF_CDECL serve_xmidi_112c10(ef_addr32 timer)
{
	if (EF_U32(timer + TIMER_LOCKED) != 0 || g_service_busy != 0)
		return;
	g_service_busy = 1;
	g_sequences_left = EF_U32(timer + TIMER_COUNT);
	g_sequence = EF_U32(timer + TIMER_SEQUENCES);
	while (g_sequences_left != 0) {
		if (SEQ(SEQ_STATUS) == 4) {
			if (!serve_sequence(timer)) {
				g_service_busy = 0;
				SEQ(SEQ_STATUS) = 2;
				return;
			}
		}
		g_sequences_left -= 1;
		g_sequence += SEQ_SIZE;
	}
	send_pending_count_message_111df0(0, 0, 0, 0, timer);
	g_service_busy = 0;
}
