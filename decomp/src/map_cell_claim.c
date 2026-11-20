/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: LicenseRef-EF2000-Derived */

#include "ef_abi.h"
#include "ef_mem.h"

#define MAP_CELLS 0x28 /* 40 x 40 cells, row-major */

/* Give the cell to GROUP when the negative group test 0xcc1ac allows it
 * for the cell's current group. */
static void claim_cell(ef_addr32 cells, int16_t index, uint8_t group, ef_addr32 head)
{
    ef_addr32 cell = cells + (uint32_t)(int32_t)index;

    if ((uint16_t)group_flag_negative_cc1ac(group, EF_U8(cell), head) != 0u)
        EF_U8(cell) = group;
}

/* 0xcb3c4, 408 bytes (ret 4). Claim the cells of the 40 x 40 map CELLS
 * (10 units per cell, y counted from 400 downward) on GROUP's side of
 * the point (X, Y). The point's row is (400 - Y) / 10, or 0 when Y has a
 * zero low word. Group 0 takes the rows from two above the point's row
 * to the bottom, in the columns left of X / 10 + 3; group 1 takes the
 * rows from one below the point's row to the top, from column
 * X / 10 - 5 to the right edge. Other groups claim nothing. */
void claim_map_cells_cb3c4(int32_t x, int32_t y, uint8_t group, ef_addr32 cells, ef_addr32 head)
{
    int16_t point_row = (int16_t)y != 0 ? (int16_t)((0x190 - y) / 10) : 0;
    int16_t column;
    int16_t row;

    if (group == 0u) {
        int32_t columns = x / 10 + 3;
        int32_t first_row = point_row - 2;

        if (columns >= MAP_CELLS)
            columns = MAP_CELLS;
        if (first_row < 0)
            first_row = 0;
        for (column = 0; column < columns; ++column)
            for (row = (int16_t)first_row; row < MAP_CELLS; ++row)
                claim_cell(cells, (int16_t)(row * MAP_CELLS + column), group, head);
    } else if (group == 1u) {
        int32_t first_column = (int16_t)(x / 10) - 5;
        int32_t last_row = point_row + 1;

        if (first_column <= 0)
            first_column = 0;
        if (last_row > MAP_CELLS - 1)
            last_row = MAP_CELLS - 1;
        for (column = (int16_t)first_column; column < MAP_CELLS; ++column) {
            if ((int16_t)last_row < 0)
                continue;
            for (row = (int16_t)last_row; row >= 0; --row)
                claim_cell(cells, (int16_t)(row * MAP_CELLS + column), group, head);
        }
    }
}
