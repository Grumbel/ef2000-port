# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Package and load verified in-place EF2000 OMF patch plans.

EFPATCH1 binary format (little endian):
    magic[8], original initialized-payload SHA256[32], count:u32,
    repeated { offset:u32, length:u32, data[length] }

Only original initialized payload bytes are writable. Arena writes are
explicitly unsupported until guest allocation high-water is established.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


MAGIC = b'EF2000P1'
HEADER = struct.Struct('<8s32sI')
ENTRY = struct.Struct('<II')
MAX_WRITES = 512
MAX_PATCH_BYTES = 16 * 1024 * 1024


class PatchError(ValueError):
    pass


def _validate_writes(writes, image_size):
    if not 1 <= len(writes) <= MAX_WRITES:
        raise PatchError('patch must contain 1..512 writes')
    intervals = []
    total = 0
    for offset, payload in writes:
        if not isinstance(offset, int) or offset < 0 or offset >= image_size:
            raise PatchError('write begins outside initialized payload')
        if not payload or len(payload) > image_size - offset:
            raise PatchError('write extends beyond initialized payload')
        total += len(payload)
        if total > MAX_PATCH_BYTES:
            raise PatchError('patch exceeds 16 MiB limit')
        intervals.append((offset, offset + len(payload)))
    intervals.sort()
    if any(b[0] < a[1] for a, b in zip(intervals, intervals[1:])):
        raise PatchError('overlapping patch writes')


def pack(plan, image):
    if plan.get('format') != 'ef2000-patch-v1':
        raise PatchError('unsupported OMF patch plan')
    digest = hashlib.sha256(image).hexdigest()
    if plan.get('base_image_sha256') != digest:
        raise PatchError('patch plan targets a different initialized payload')
    writes = []
    for write in plan.get('writes', []):
        if write.get('kind') != 'inline':
            raise PatchError('arena/redirected patches require verified runtime reservation')
        try:
            data = bytes.fromhex(write['bytes'])
            offset = write['offset']
        except (KeyError, TypeError, ValueError) as error:
            raise PatchError('invalid patch write') from error
        writes.append((offset, data))
    _validate_writes(writes, len(image))
    out = bytearray(HEADER.pack(MAGIC, bytes.fromhex(digest), len(writes)))
    for offset, data in writes:
        out.extend(ENTRY.pack(offset, len(data)))
        out.extend(data)
    return bytes(out)


def decode(contents, image):
    if len(contents) < HEADER.size or len(contents) > MAX_PATCH_BYTES + 8192:
        raise PatchError('invalid EFPATCH1 file size')
    magic, digest, count = HEADER.unpack_from(contents)
    if magic != MAGIC:
        raise PatchError('invalid EFPATCH1 magic')
    if digest != hashlib.sha256(image).digest():
        raise PatchError('patch belongs to a different initialized payload')
    if not 1 <= count <= MAX_WRITES:
        raise PatchError('invalid EFPATCH1 write count')
    cursor = HEADER.size
    writes = []
    for _ in range(count):
        if cursor + ENTRY.size > len(contents):
            raise PatchError('truncated EFPATCH1 write header')
        offset, size = ENTRY.unpack_from(contents, cursor)
        cursor += ENTRY.size
        if size > len(contents) - cursor:
            raise PatchError('truncated EFPATCH1 write')
        writes.append((offset, contents[cursor:cursor + size]))
        cursor += size
    if cursor != len(contents):
        raise PatchError('trailing EFPATCH1 bytes')
    _validate_writes(writes, len(image))
    return writes


def apply_patches(image, paths):
    """Return modified initialized image and ordered patch digests.

    The original image is used for *every* patch identity check. File writes
    are fully checked for inter-patch overlap before a single byte is changed.
    """
    originals = bytes(image)
    writes = []
    digests = []
    for path in paths:
        blob = Path(path).read_bytes()
        writes.extend(decode(blob, originals))
        digests.append(hashlib.sha256(blob).hexdigest())
    if writes:
        _validate_writes(writes, len(originals))
    patched = bytearray(originals)
    for offset, data in writes:
        patched[offset:offset + len(data)] = data
    return bytes(patched), digests


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plan', type=Path, help='offline omf_splice.py JSON plan')
    parser.add_argument('--image', required=True, type=Path,
                        help='original unpatched initialized X-32 payload')
    parser.add_argument('-o', '--output', required=True, type=Path,
                        help='compiled in-place EFPATCH1 binary')
    args = parser.parse_args()
    try:
        blob = pack(json.loads(args.plan.read_text()), args.image.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(blob)
        print(f'EFPATCH1: {args.output} ({len(blob)} bytes)')
    except (OSError, PatchError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
