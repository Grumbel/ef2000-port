# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Find embedded [handler offset, callback name pointer] tables in the X-32 payload.

Rows describe evidence from the original image, not verified function types.
The importer validates the SHA-pinned payload before calling this module.
"""

import re
import struct


NAME_PATTERN = re.compile(rb'[A-Za-z_][A-Za-z0-9_]{2,63}\Z')
MIN_NAME_ADDRESS = 0x170000


def read_name(image, offset):
    """Return a C identifier at a data address, excluding arbitrary byte strings."""
    if not MIN_NAME_ADDRESS <= offset < len(image):
        return None
    candidate = image[offset:offset + 65]
    end = candidate.find(b'\0')
    if end < 0 or not NAME_PATTERN.fullmatch(candidate[:end]):
        return None
    return candidate[:end].decode('ascii')


def find_tables(image, minimum=4):
    """Return [(start, [(slot, handler, name_address, name), ...]), ...]."""
    if minimum < 2:
        raise ValueError('callback table minimum must be at least two records')
    tables = []
    cursor = 0
    last = len(image) - 8
    names = {}
    while cursor <= last:
        rows = []
        scan = cursor
        while scan <= last:
            handler, name_address = struct.unpack_from('<II', image, scan)
            if handler >= len(image):
                break
            if name_address not in names:
                names[name_address] = read_name(image, name_address)
            name = names[name_address]
            if name is None:
                break
            rows.append((scan, handler, name_address, name))
            scan += 8
        if len(rows) >= minimum:
            tables.append((cursor, rows))
            cursor = scan
        else:
            cursor += 4
    return tables


def select_function_tables(tables, function_entries):
    """Require substantial exact function-entry evidence for a candidate table.

    EF2000 also contains [data pointer, string pointer] tables for pilot
    names, enums, and UI labels. An initialized-image address is not on its
    own proof of executable code, so use Ghidra's function inventory to
    discriminate. Rows with as-yet-unrecognized handlers remain in a table
    once its overall structure has been independently corroborated.
    """
    selected = []
    for start, rows in tables:
        matched = sum(handler in function_entries for _, handler, _, _ in rows)
        if matched >= 2 and matched * 3 >= len(rows):
            selected.append((start, rows))
    return selected
