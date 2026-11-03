#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Read EF2000 DOS DID.DAT / DID1ST.DAT hashed archive indices.

The original files must be supplied by the caller. Archive members do not
contain their filenames, so a game pathname is needed to resolve an entry.
"""

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct


@dataclass(frozen=True)
class Entry:
    hash: int
    offset: int
    size: int


def hash_path(encoded):
    """Compute the original EF2000/TACTCOM 15-byte resource-path hash.

    `encoded` contains a one-byte directory code (when present), the ASCII
    base filename, and the one-byte extension code (when it fits).
    """
    if len(encoded) > 15:
        raise ValueError('encoded resource path exceeds 15 bytes')
    rolling = 0
    low = 0
    high = 0
    bitmap = 0
    previous = 0
    for index, char in enumerate(encoded):
        if index and previous != ord('_'):
            bitmap = (bitmap << 1) & 0xff
            if previous < char:
                bitmap |= 1
        previous = char
        if char == ord('_'):
            rolling = (rolling - index) & 0xff
        else:
            low = (low + char) & 0xff
            high = (high + low) & 0xff
            rolling = (char ^ (rolling * 2)) & 0xff
    return (((len(encoded) & 15) << 28) | ((low & 15) << 24) |
            (high << 16) | (rolling << 8) | bitmap)


class DIDArchive:
    def __init__(self, data):
        self.data = data
        if len(data) < 16:
            raise ValueError('archive too small')
        offset = struct.unpack_from('<I', data)[0]
        if offset < 8 or offset > len(data) - 8:
            raise ValueError('archive directory offset outside file')
        count, n_dirs, n_exts, table_bytes = struct.unpack_from('<4H', data, offset)
        if not count or count > 16384 or n_dirs > 255 or n_exts > 255:
            raise ValueError('implausible archive directory counts')
        start = offset + 8
        end_names = start + table_bytes
        end_index = end_names + 12 * count
        if end_index != len(data):
            raise ValueError('archive directory/index length mismatch')

        def read_names(number):
            nonlocal start
            result = []
            for _ in range(number):
                if start >= end_names:
                    raise ValueError('truncated archive name table')
                length = data[start]
                start += 1
                if not length or start + length > end_names:
                    raise ValueError('invalid archive name length')
                try:
                    result.append(data[start:start + length].decode('ascii'))
                except UnicodeDecodeError as error:
                    raise ValueError('non-ASCII archive name') from error
                start += length
            return result

        self.directories = read_names(n_dirs)
        self.extensions = read_names(n_exts)
        if start != end_names:
            raise ValueError('archive name table contains unexpected bytes')
        self.entries = []
        self.by_hash = {}
        previous = -2**31
        for index in range(count):
            hashed, position, size = struct.unpack_from('<III', data, end_names + 12 * index)
            signed_hash = hashed if hashed < 0x80000000 else hashed - 2**32
            if signed_hash < previous or position < 4 or size > offset - position:
                raise ValueError('unsorted hash or out-of-bounds archive member')
            previous = signed_hash
            entry = Entry(hashed, position, size)
            self.entries.append(entry)
            if hashed in self.by_hash:
                raise ValueError('duplicate archive hash')
            self.by_hash[hashed] = entry

    @classmethod
    def from_path(cls, filename):
        return cls(Path(filename).read_bytes())

    def encode_path(self, filename):
        path = filename.replace('/', '\\').lower()
        if not path or '\0' in path or path.startswith('\\'):
            raise ValueError('invalid guest pathname')
        directory, separator, basename = path.rpartition('\\')
        directory = (directory + '\\') if separator else ''
        stem, dot, extension = basename.rpartition('.')
        if not dot:
            stem = basename
        else:
            extension = '.' + extension
        if not stem:
            raise ValueError('empty resource name')
        try:
            encoded = bytes([ord('0') + self.directories.index(directory)]) if directory else b''
            encoded += stem.encode('ascii')[:15 - len(encoded)]
            if extension and len(encoded) < 15:
                encoded += bytes([ord('A') + self.extensions.index(extension)])
        except (ValueError, UnicodeEncodeError) as error:
            raise ValueError(f'directory or extension not found in archive: {filename}') from error
        return encoded

    def lookup(self, filename):
        return self.by_hash.get(hash_path(self.encode_path(filename)))

    def raw_member(self, entry):
        return self.data[entry.offset:entry.offset + entry.size]

    def member(self, entry):
        blob = self.raw_member(entry)
        if len(blob) >= 12 and blob[:2] == b'RA':
            try:
                from .ra_decode import decompress
            except ImportError:
                from ra_decode import decompress
            return decompress(blob)
        return blob

    def member_head(self, entry, limit=12):
        blob = self.raw_member(entry)
        if len(blob) >= 12 and blob[:2] == b'RA':
            try:
                from .ra_decode import decompress
            except ImportError:
                from ra_decode import decompress
            return decompress(blob, prefix=limit)
        return blob[:limit]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('paths', nargs='*', help='guest member names (e.g. iff/256/newgui03.lbm)')
    parser.add_argument('--out', type=Path, help='extract resolved files into this directory')
    args = parser.parse_args(argv)
    try:
        archive = DIDArchive.from_path(args.archive)
        print(f'{args.archive}: {len(archive.entries)} members; '
              f'{len(archive.directories)} directories; {len(archive.extensions)} extensions')
        if not args.paths:
            print('directories:', ', '.join(archive.directories))
            print('extensions:', ', '.join(archive.extensions))
        for name in args.paths:
            entry = archive.lookup(name)
            if entry is None:
                print(f'missing: {name}')
                continue
            blob = archive.raw_member(entry)
            compressed = 'RA' if blob.startswith(b'RA') else 'raw'
            print(f'{name}: hash={entry.hash:08x} size={entry.size} {compressed}')
            if args.out:
                relative = Path(name.replace('\\', '/'))
                if relative.is_absolute() or '..' in relative.parts:
                    raise ValueError('refusing unsafe output path')
                output = args.out / relative
                content = archive.member(entry)
                output.parent.mkdir(parents=True, exist_ok=True)
                with output.open('xb') as stream:
                    stream.write(content)
    except (ValueError, OSError, NotImplementedError) as error:
        parser.exit(1, f'did_archive: {error}\n')


if __name__ == '__main__':
    main()
