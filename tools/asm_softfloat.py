#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Build a pinned, independent 80-bit arithmetic dependency under ignored local/.

Supply the official SoftFloat-3e.zip; this tool never downloads or vendors it.
This builds a library, not an x87 architectural-state implementation.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE_SHA256 = '21130ce885d35c1fe73fc1e1bf2244178167e05c6747cad5f450cc991714c746'
SOURCE_URL = 'https://www.jhauser.us/arithmetic/SoftFloat-3e.zip'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def extract(archive, destination):
    if digest(archive) != ARCHIVE_SHA256:
        raise ValueError('SoftFloat archive digest mismatch')
    with zipfile.ZipFile(archive) as package:
        for info in package.infolist():
            path = Path(info.filename)
            if (path.is_absolute() or '..' in path.parts or
                    not path.parts or path.parts[0] != 'SoftFloat-3e' or
                    stat.S_ISLNK(info.external_attr >> 16)):
                raise ValueError('unsafe SoftFloat archive member')
        package.extractall(destination)


def remove_unused_label(source):
    """Remove one upstream label with no goto; retain all statements/notices."""
    text = source.read_text()
    if re.search(r'\bgoto\s+roundPack\b', text) or text.count('\n roundPack:\n') != 1:
        raise ValueError('unexpected SoftFloat unused-label repair input')
    source.write_text(text.replace('\n roundPack:\n', '\n'))


def build(archive, output, *, compiler='cc', watcom=None):
    if output.exists():
        raise ValueError('use a fresh SoftFloat build directory')
    # Verify before creating anything, including when callers use this API.
    if digest(archive) != ARCHIVE_SHA256:
        raise ValueError('SoftFloat archive digest mismatch')
    output.mkdir(parents=True)
    extract(archive, output)
    upstream = output / 'SoftFloat-3e'
    source = upstream / 'source'
    repaired = source / 's_addExtF80M.c'
    original_digest = digest(repaired)
    remove_unused_label(repaired)
    platform = output / 'platform.h'
    platform.write_text('#define LITTLEENDIAN 1\n#define INLINE inline\n')
    makefile = upstream / 'build/Linux-386-GCC/Makefile'
    names = sorted(set(re.findall(r'\b(\w+)\$\(OBJ\)', makefile.read_text())))
    if len(names) != 237:
        raise ValueError('unexpected SoftFloat source inventory')
    includes = [output, source / '8086', source / 'include']
    host_objects, records = [], []
    for name in names:
        unit = source / '8086' / (name + '.c')
        if not unit.exists():
            unit = source / (name + '.c')
        host = output / (name + '.o')
        command = [compiler, '-c', '-fPIC', '-O2', '-std=c99', '-Werror',
                   '-DSOFTFLOAT_ROUND_ODD', '-DINLINE_LEVEL=0']
        command += ['-I' + str(p) for p in includes] + [str(unit), '-o', str(host)]
        result = subprocess.run(command, capture_output=True, text=True)
        (output / (name + '.host.log')).write_text(result.stdout + result.stderr)
        result.check_returncode()
        record = dict(source=str(unit.relative_to(output)), source_sha256=digest(unit),
                      host_object_sha256=digest(host), host_command=command)
        host_objects.append(host)
        if watcom:
            obj = output / (name + '.obj')
            command = [watcom, '-q', '-bt=dos', '-3r', '-za99', '-w1', '-we',
                       '-dSOFTFLOAT_ROUND_ODD', '-dINLINE_LEVEL=0']
            command += ['-i=' + str(p) for p in includes] + ['-fo=' + str(obj), str(unit)]
            result = subprocess.run(command, capture_output=True, text=True)
            (output / (name + '.watcom.log')).write_text(result.stdout + result.stderr)
            result.check_returncode()
            record.update(watcom_object_sha256=digest(obj), watcom_command=command)
        records.append(record)
    library = output / 'softfloat.so'
    command = [compiler, '-shared', '-Wl,-z,defs', '-o', str(library)] + [str(p) for p in host_objects]
    result = subprocess.run(command, capture_output=True, text=True)
    (output / 'link.log').write_text(result.stdout + result.stderr)
    result.check_returncode()
    report = dict(schema=1, source_url=SOURCE_URL, archive_sha256=digest(archive),
                  specialization='8086', fast_int64=False, inline_level=0, round_odd=True,
                  platform_sha256=digest(platform), library_sha256=digest(library),
                  headers={str(p.relative_to(output)): digest(p) for p in sorted(source.rglob('*.h'))},
                  repair=dict(file=str(repaired.relative_to(output)),
                              before_sha256=original_digest, after_sha256=digest(repaired),
                              operation='remove unreferenced roundPack label only'),
                  sources=records)
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def validated_build(directory, *, watcom=False):
    report = json.loads((directory / 'report.json').read_text())
    if (not isinstance(report, dict) or report.get('schema') != 1 or report.get('archive_sha256') != ARCHIVE_SHA256 or
            report.get('specialization') != '8086' or report.get('fast_int64') is not False or
            report.get('inline_level') != 0 or report.get('round_odd') is not True or
            not isinstance(report.get('headers'), dict) or
            not isinstance(report.get('sources'), list) or len(report['sources']) != 237 or
            not all(isinstance(e, dict) and isinstance(e.get('source'), str) and
                    isinstance(e.get('source_sha256'), str) for e in report['sources']) or
            len({e['source'] for e in report['sources']}) != 237):
        raise ValueError('incompatible SoftFloat build profile')
    files = {'platform.h': report.get('platform_sha256'), 'softfloat.so': report.get('library_sha256')}
    if set(files) & set(report['headers']):
        raise ValueError('duplicate SoftFloat artifact binding')
    files.update(report['headers'])
    for entry in report['sources']:
        if entry['source'] in files:
            raise ValueError('duplicate SoftFloat source binding')
        files[entry['source']] = entry['source_sha256']
        if watcom:
            files[Path(entry['source']).stem + '.obj'] = entry.get('watcom_object_sha256')
    for name, expected in files.items():
        if not isinstance(name, str) or not isinstance(expected, str):
            raise ValueError('invalid SoftFloat artifact record')
        path = (directory / name).resolve()
        if not path.is_relative_to(directory.resolve()) or not expected or digest(path) != expected:
            raise ValueError('SoftFloat artifact digest/path mismatch: ' + name)
    return report


def build_narrow(directory, output, *, compiler='cc', watcom=None):
    return build_adapter(directory, output, 'narrow', compiler=compiler, watcom=watcom)


def build_binary(directory, output, *, compiler='cc', watcom=None):
    return build_adapter(directory, output, 'binary', compiler=compiler, watcom=watcom)


def build_adapter(directory, output, name, *, compiler='cc', watcom=None):
    if name not in ('narrow', 'binary'):
        raise ValueError('unknown SoftFloat adapter')
    report = validated_build(directory, watcom=bool(watcom))
    if output.exists():
        raise ValueError('use a fresh SoftFloat adapter build directory')
    output.mkdir(parents=True)
    source = ROOT / ('tools/asm_x87_%s.c' % name)
    includes = [directory, directory / 'SoftFloat-3e/source/include']
    library = output / (name + '.so')
    command = [compiler, '-shared', '-fPIC', '-O2', '-std=c99', '-Wall', '-Wextra', '-Werror']
    command += ['-I' + str(p) for p in includes]
    command += [str(source), str(directory / 'softfloat.so'), '-Wl,-z,defs', '-o', str(library)]
    result = subprocess.run(command, capture_output=True, text=True)
    (output / 'host.log').write_text(result.stdout + result.stderr)
    result.check_returncode()
    evidence = dict(schema=1, model='x87-%s-softfloat-3e-8086' % name,
                    dependency_report_sha256=digest(directory / 'report.json'),
                    dependency_library_sha256=report['library_sha256'],
                    source_sha256=digest(source), library_sha256=digest(library), host_command=command)
    if watcom:
        obj = output / (name + '.obj')
        command = [watcom, '-q', '-bt=dos', '-3r', '-za99', '-w1', '-we']
        command += ['-i=' + str(p) for p in includes] + ['-fo=' + str(obj), str(source)]
        result = subprocess.run(command, capture_output=True, text=True)
        (output / 'watcom.log').write_text(result.stdout + result.stderr)
        result.check_returncode()
        evidence.update(watcom_object_sha256=digest(obj), watcom_command=command)
    (output / 'report.json').write_text(json.dumps(evidence, indent=2) + '\n')
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument('--archive', type=Path)
    inputs.add_argument('--narrow-from', type=Path, help='build adapter from a validated dependency build')
    inputs.add_argument('--binary-from', type=Path, help='build masked arithmetic adapter')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', default='cc')
    parser.add_argument('--watcom')
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        if not output.is_relative_to(ROOT / 'local'):
            raise ValueError('SoftFloat artifacts must stay under ignored local/')
        if args.binary_from:
            build_binary(args.binary_from.resolve(), output, compiler=args.compiler, watcom=args.watcom)
            print('Built SoftFloat arithmetic adapter; output: %s' % output)
        elif args.narrow_from:
            build_narrow(args.narrow_from.resolve(), output, compiler=args.compiler, watcom=args.watcom)
            print('Built SoftFloat narrowing adapter; output: %s' % output)
        else:
            report = build(args.archive.resolve(), output, compiler=args.compiler, watcom=args.watcom)
            print('Built %d SoftFloat sources; output: %s' % (len(report['sources']), output))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'asm_softfloat: %s\n' % error)


if __name__ == '__main__':
    main()
