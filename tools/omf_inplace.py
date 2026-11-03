#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Build a SHA-bound in-place EFPATCH1 from one reviewed OpenWatcom object.

This intentionally cannot allocate an executable arena or redirect guest
control flow. A larger replacement, unrecognized relocation, unconfirmed ABI,
or object with private initialized data fails before a patch file is created.
The original game payload is never modified.
"""

import argparse
from pathlib import Path

try:
    from .efpatch import pack, PatchError
    from .flirt_match import OMFError
    from .highscores_differential import load_image
    from .omf_splice import parse_symbols, plan_patch, read_module
except ImportError:
    from efpatch import pack, PatchError
    from flirt_match import OMFError
    from highscores_differential import load_image
    from omf_splice import parse_symbols, plan_patch, read_module


ROOT = Path(__file__).resolve().parents[1]


def build(object_bytes, image, functions, globals_, reviewed):
    """Return only a validated in-place patch, not an intermediate plan."""
    if not reviewed:
        raise OMFError('explicitly reviewed control-flow address required')
    module = read_module(object_bytes)
    # plan_patch only requires a notional arena address; pack() rejects any
    # redirect or private-data write, so that arena is never referenced.
    plan = plan_patch(module, image, functions, globals_, len(image), set(reviewed))
    blob = pack(plan, image)
    if len(plan['writes']) != 1 or plan['writes'][0]['kind'] != 'inline':
        raise PatchError('one in-place code replacement required')
    return blob, plan['writes'][0]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object', type=Path, help='compiled OpenWatcom OMF .obj')
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--executable', type=Path, help='SHA-pinned original _EF2000.EXE')
    source.add_argument('--payload', type=Path, help='SHA-pinned extracted payload.bin')
    parser.add_argument('--reviewed-flow', type=lambda s: int(s, 0), required=True,
                        help='address with independently reviewed body and control flow')
    parser.add_argument('--functions', type=Path,
                        default=ROOT / 'decomp/symbols/functions.tsv')
    parser.add_argument('--globals', type=Path,
                        default=ROOT / 'decomp/symbols/globals.tsv')
    parser.add_argument('-o', '--output', type=Path, required=True,
                        help='new EFPATCH1 file (will never overwrite)')
    args = parser.parse_args(argv)
    try:
        image = load_image(executable=args.executable, payload_path=args.payload)
        blob, write = build(args.object.read_bytes(), image,
                            parse_symbols(args.functions), parse_symbols(args.globals),
                            {args.reviewed_flow})
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('xb') as stream:
            stream.write(blob)
        print(f'EFPATCH1: 0x{write["offset"]:x} in-place, '
              f'{len(bytes.fromhex(write["bytes"]))} bytes -> {args.output}')
    except (OSError, OMFError, PatchError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
