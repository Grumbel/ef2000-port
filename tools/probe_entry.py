#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Execute the original entry under QEMU, stopping at its first unsupported INT.

This is a diagnostic probe, not a DOS runtime. Its ELF contains game bytes and
must stay in ignored local/ or build/ directories.
"""

import argparse
import json
from pathlib import Path
import re
import resource
import shutil
import signal
import struct
import subprocess

try:
    from .extract_image import extract
except ImportError:
    from extract_image import extract


def make_elf(payload):
    """Wrap the flat image in ELF32 for a synthetic zero-based QEMU probe."""
    if not (0x95 <= len(payload) <= 0x7ff000):
        raise ValueError('payload does not fit probe memory layout')
    payload_offset = 0x1000
    wrapper_offset = (payload_offset + len(payload) + 0xfff) & ~0xfff
    wrapper = bytes.fromhex('bc 00 f0 7f 00 b8 86 00 00 00 ff e0')
    ident = b'\x7fELF\x01\x01\x01' + bytes(9)
    header = struct.pack('<16sHHIIIIIHHHHHH', ident, 2, 3, 1, 0x08000000,
                         52, 0, 0, 52, 32, 2, 0, 0, 0)
    guest = struct.pack('<8I', 1, payload_offset, 0, 0, len(payload), 0x800000, 7, 0x1000)
    host = struct.pack('<8I', 1, wrapper_offset, 0x08000000, 0x08000000,
                       len(wrapper), len(wrapper), 5, 0x1000)
    result = bytearray(wrapper_offset + len(wrapper))
    result[:len(header + guest + host)] = header + guest + host
    result[payload_offset:payload_offset + len(payload)] = payload
    result[wrapper_offset:] = wrapper
    return bytes(result)


def first_boundary(trace, returncode):
    """Require both the expected instruction boundary and register state."""
    frames = trace.split('----------------')
    for frame in reversed(frames):
        values = dict((name, int(value, 16)) for name, value in
                      re.findall(r'\b(EAX|EBX|ECX|EDX|ESI|EDI|EBP|ESP|EIP|EFL)=([0-9a-fA-F]{8})', frame))
        if values.get('EIP') != 0x93:
            continue
        expected = (values.get('EAX') == 0x3505 and values.get('ECX') == 0x260 and
                    values.get('ESP') == 0x7ff000 and
                    re.search(r'0x0*93:\s+cd 21\s+int\s+\$0x21', frame) is not None and
                    returncode == -signal.SIGSEGV)
        return values if expected else None
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--qemu', default='qemu-i386')
    args = parser.parse_args()
    qemu = shutil.which(args.qemu)
    if qemu is None:
        parser.error(f'QEMU executable not found: {args.qemu}')
    try:
        manifest, images = extract(args.executable.read_bytes())
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=False)
        elf = output / 'probe.elf'
        elf.write_bytes(make_elf(images['payload.bin']))
        elf.chmod(0o700)
        command = [qemu, '-B', '0x40000000', '-one-insn-per-tb', '-d', 'in_asm,cpu',
                   '-D', str(output / 'trace.log'), str(elf)]
        # The unsupported software interrupt intentionally raises target SIGSEGV.
        # Disable core files: they would contain original game bytes.
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        with (output / 'console.log').open('w') as console:
            run = subprocess.run(command, cwd=output, stdout=console,
                                 stderr=subprocess.STDOUT, timeout=10, check=False)
        trace = (output / 'trace.log').read_text()
        registers = first_boundary(trace, run.returncode)
        version = subprocess.run([qemu, '--version'], capture_output=True,
                                 text=True, timeout=5, check=True).stdout.splitlines()[0]
        report = {
            'source_sha256': manifest['source_sha256'],
            'qemu_version': version,
            'command': command,
            'returncode': run.returncode,
            'expected_boundary_reached': registers is not None,
            'registers_at_boundary': registers,
            'interface': 'INT 21h AX=3505h (FlashTek X-32 selector limit operation)',
            'limitations': [
                'Synthetic Linux-user-mode selectors and stack; not a DOS reference snapshot.',
                'Only three original instructions execute before the unsupported INT.',
                'No interrupt handler is installed; SIGSEGV is the expected diagnostic stop.',
                'PSP, DOS environment, extender patches, and hardware are not initialized.',
            ],
        }
        (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'{error}\n')
    if registers is None:
        parser.exit(1, f'Expected boundary not verified; inspect {output}\n')
    print(f'Original entry reached INT 21h AX=3505h at image offset 0x93; see {output}')


if __name__ == '__main__':
    main()
