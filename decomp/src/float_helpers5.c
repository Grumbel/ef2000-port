/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_globals.h"
#include "ef_mem.h"

/* Fifth x87 batch: the 0xb4xxx math library wrappers and a few record scalers. */

#define EF_F64(a) (*(double *)ef_guest_memory_ptr((ef_addr32)(a)))

/* 0x9ffc8, 80 bytes. Convert a table entry's vector and scale a word into a float. */
void convert_entry_vector_9ffc8(int16_t index, int16_t value)
{
    ef_addr32 entry = EF_U32(0x0041717cu) + (uint32_t)(int32_t)index * 6u;

    guest_fn_ea9a0(0x00132b28u, EF_S16(entry + 0xau), EF_S32(entry + 0xau) >> 16, EF_S32(entry + 0xcu) >> 16);
    *(float *)ef_guest_memory_ptr(0x00132b34u) = (float)((double)value * EF_F64(0x0018b308u));
}

/* 0xa5508, 132 bytes. Set a record's offsets from two scaled values and refresh it. */
void set_record_offsets_a5508(ef_addr32 record, int16_t a, int16_t b)
{
    int32_t half;
    double v;

    half = (EF_S32(record + 0x73u) >> 16) / 2;
    v = (double)a * EF_F64(record + 0x9fu) - (double)half;
    EF_U16(record + 0x69u) = (uint16_t)(int32_t)v;
    half = (EF_S32(record + 0x75u) >> 16) / 2;
    v = (double)b * EF_F64(record + 0x9fu) - (double)half;
    EF_U16(record + 0x6bu) = (uint16_t)(int32_t)v;
    guest_fn_a558c(record);
    guest_fn_a5ef8(EF_U8(record + 0x10u));
}

/* 0xa56b0, 69 bytes. Store a positive scale factor in a record and redraw it. */
void set_record_scale_a56b0(ef_addr32 record, double scale)
{
    if (scale > EF_F64(0x0018cffeu)) {
        EF_F64(record + 0x9fu) = scale;
        guest_fn_a520c(record, EF_S32(record + 0x43u) >> 16, EF_S32(record + 0x45u) >> 16, 0);
    }
}

/* 0xb415c, 93 bytes. Distance between two 2D double points, 0.0 when degenerate. */
double distance_2d_b415c(ef_addr32 a, ef_addr32 b)
{
    double sum = guest_fn_b3fd0(EF_F64(a) - EF_F64(b));

    sum = guest_fn_b3fd0(EF_F64(a + 8u) - EF_F64(b + 8u)) + sum;
    if (0.0 >= sum)
        return 0.0;
    return guest_fn_a4470(sum);
}

/* 0xb41bc, 116 bytes. Distance between two 3D double points, 0.0 when degenerate. */
double distance_3d_b41bc(ef_addr32 a, ef_addr32 b)
{
    double sum = guest_fn_b3fd0(EF_F64(a) - EF_F64(b));

    sum = guest_fn_b3fd0(EF_F64(a + 8u) - EF_F64(b + 8u)) + sum;
    sum = guest_fn_b3fd0(EF_F64(a + 0x10u) - EF_F64(b + 0x10u)) + sum;
    if (0.0 >= sum)
        return 0.0;
    return guest_fn_a4470(sum);
}

/* 0xb4230, 31 bytes. Convert a signed word to a scaled double. */
double word_to_scaled_b4230(int16_t value)
{
    return ((double)value * EF_F64(0x0018e865u) + EF_F64(0x0018e86du)) * EF_F64(0x0018e875u);
}

/* 0xb4250, 42 bytes. Convert a double to a scaled integer. */
int32_t double_to_scaled_b4250(double value)
{
    return (int32_t)((value * EF_F64(0x0018e87du) + EF_F64(0x0018e885u)) * EF_F64(0x0018e88du));
}

/* 0xb427c, 51 bytes. Absolute value of a double. */
double abs_double_b427c(double value)
{
    if (0.0 >= value)
        return -value;
    return value;
}

/* 0xb42b0, 63 bytes. Sign of a double as +1.0 or -1.0 (zero gives -1.0). */
double sign_double_b42b0(double value)
{
    if (0.0 >= value)
        return -1.0;
    return 1.0;
}

/* 0xb42f0, 113 bytes. Wrap an angle in degrees into its canonical range. */
double wrap_angle_b42f0(double value)
{
    if (value > EF_F64(0x0018e895u) || value < EF_F64(0x0018e89du))
        value = 0.0;
    while (value > EF_F64(0x0018e8a5u))
        value = value + EF_F64(0x0018e8adu);
    while (!(value > EF_F64(0x0018e8b5u)))
        value = value + EF_F64(0x0018e8bdu);
    return value;
}

/* 0xb4364, 118 bytes. Length of a 3D vector of doubles, 0.0 when degenerate. */
double vector_length_b4364(ef_addr32 vec)
{
    double sum = guest_fn_b3fd0(EF_F64(vec));

    sum = guest_fn_b3fd0(EF_F64(vec + 8u)) + sum;
    sum = guest_fn_b3fd0(EF_F64(vec + 0x10u)) + sum;
    if (0.0 >= sum)
        return 0.0;
    return guest_fn_a4470(sum);
}

/* 0x4c6a0, 119 bytes. Zoom the active map widget out by 0.2 while below 4.0
 * and publish the resulting zoom level as a float. */
void mp_zoom_out_map_4c6a0(void)
{
    ef_addr32 map = guest_fn_a46cc(EF_U8(0x1a53dcu) == 0u ? 0x00184c38u : 0x00184a40u);

    if (EF_F64(map + 0x9fu) < EF_F64(0x00184c3cu)) {
        set_record_scale_a56b0(map, EF_F64(map + 0x9fu) + EF_F64(0x00184c44u));
        set_record_offsets_a5508(map, 0xc8, 0xc8);
    }
    *(float *)ef_guest_memory_ptr(0x001a53dfu) = (float)((EF_F64(map + 0x9fu) + EF_F64(0x00184c4cu)) * EF_F64(0x00184c54u) + 1.0);
}

/* 0x4c718, 115 bytes. Zoom the active map widget in by 0.2 while above 1.0
 * and publish the resulting zoom level as a float. */
void mp_strike_list_selected_4c718(void)
{
    ef_addr32 map = guest_fn_a46cc(EF_U8(0x1a53dcu) == 0u ? 0x00184c38u : 0x00184a40u);

    if (1.0 < EF_F64(map + 0x9fu)) {
        set_record_scale_a56b0(map, EF_F64(map + 0x9fu) + EF_F64(0x00184c5cu));
        set_record_offsets_a5508(map, 0xc8, 0xc8);
    }
    *(float *)ef_guest_memory_ptr(0x001a53dfu) = (float)((EF_F64(map + 0x9fu) + EF_F64(0x00184c64u)) * EF_F64(0x00184c6cu) + 1.0);
}
