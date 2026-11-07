#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Mechanically turn exported Ghidra pseudo-C into compilable draft C.

Input is the local, git-ignored Ghidra export (``local/ghidra/full-export``).
Output goes ONLY under ``local/mechanical/`` (git-ignored): the result is a
derived work of Ghidra output and must never be committed (plan rule 2).
Hand-written, reviewed bodies in ``decomp/src`` always win over the output
(see ``--skip-recovered``).

Pipeline (docs/decompilation-plan-rev3.md, M0):
  1. normalise Ghidra types and names (FUN_x -> ef_fn_x, DAT_x -> EF_U32(..));
  2. apply confirmed signatures from decomp/symbols/signatures.tsv as
     ``#pragma aux`` register conventions in one shared header;
  3. emit one .c per function plus ``ef_gen_abi.h``;
  4. optionally compile each with OpenWatcom (``--compile``) and write
     ``report.tsv`` (state: ok / compile-error / manual).

Functions that use constructs we cannot translate mechanically (software
interrupts, ``in_*``/``unaff_*`` registers, CONCAT/SUB pieces, inline asm
markers) are flagged ``manual`` and not emitted for compilation.
"""

import argparse
import csv
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
EXPORT = ROOT / 'local/ghidra/full-export'
OUT = ROOT / 'local/mechanical'
SIGNATURES = ROOT / 'decomp/symbols/signatures.tsv'
FUNCTIONS = ROOT / 'decomp/symbols/functions.tsv'
GLOBALS = ROOT / 'decomp/symbols/globals.tsv'
FLAGS = ('-bt=dos', '-3r', '-zp1', '-fpi87', '-fp3', '-ob', '-ot', '-s', '-za99', '-w1')

TYPES = [
    # Ghidra uses undefined6/uint6 for values returned in EDX:EAX where only
    # the low 16 bits of EDX are significant.  uint64_t is the compiler's
    # available carrier; casts and consumers select the observed 48-bit parts.
    (r'\bundefined8\b', 'uint64_t'), (r'\bundefined6\b', 'uint64_t'),
    (r'\buint6\b', 'uint64_t'), (r'\buint3\b', 'uint32_t'),
    (r'\bundefined4\b', 'uint32_t'),
    (r'\bundefined3\b', 'uint32_t'), (r'\bundefined2\b', 'uint16_t'),
    (r'\bundefined1\b', 'uint8_t'), (r'\bundefined\b', 'uint8_t'),
    (r'\bulonglong\b', 'uint64_t'), (r'\blonglong\b', 'int64_t'),
    (r'\bfloat10\b', 'long double'),
    (r'\bdword\b', 'uint32_t'),
    (r'\bcode\b', 'ef_gen_code'),
    (r'\bushort\b', 'uint16_t'), (r'\buint\b', 'uint32_t'),
    (r'\bulong\b', 'uint32_t'), (r'\bbyte\b', 'uint8_t'),
    (r'\bbool\b', 'int'), (r'\bint3\b', 'int32_t'), (r'\bint5\b', 'int32_t'),
    (r'\b__(?:fastcall|stdcall|cdecl|thiscall)\b', ''),
]
# Anything matching these cannot be translated mechanically.
MANUAL = re.compile(r'\b(?:swi|in_[A-Za-z]+|unaff_[A-Za-z0-9]+|extraout_[A-Za-z0-9]+|'
                    r'SUB\d+|ZEXT\d+|SEXT\d+|halt_baddata|halt_unimplemented|'
                    r'WARNING|bad_instruction|CARRY\d|SBORROW\d|__halt)\b')
HEX = r'([0-9a-fA-F]{8})'
DEF = re.compile(r'^(?P<ret>[A-Za-z_][\w\s\*]*?)\s*\b(?P<name>\w+)\((?P<params>[^()]*)\)\s*$')


def read_tsv(path):
    with open(path, newline='') as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def manual_cause(token):
    """Group an unsupported Ghidra token into a reportable manual cause."""
    if token == 'swi':
        return 'software interrupt'
    if token.startswith('in_'):
        return 'register live-in'
    if token.startswith('unaff_'):
        return 'unpreserved register'
    if token.startswith('extraout_'):
        return 'extra return register'
    if token.startswith(('SUB', 'ZEXT', 'SEXT')):
        return 'bitfield/extract operation'
    if token.startswith(('CARRY', 'SBORROW')):
        return 'flag arithmetic'
    if token.startswith('halt_') or token == '__halt':
        return 'unimplemented control flow'
    if token in ('WARNING', 'bad_instruction'):
        return 'unsupported instruction/decompiler warning'
    return 'unsupported construct'


def normalize_type(ctype):
    for pattern, replacement in TYPES:
        ctype = re.sub(pattern, replacement, ctype)
    return ctype


def recovered_addresses():
    """Addresses that already have hand-written bodies (state compiles+)."""
    skip = {'unknown', 'named', 'typed', 'drafted', 'blocked', 'compiles-bad', ''}
    return {int(row['addr'], 16) for row in read_tsv(FUNCTIONS)
            if row['state'] not in skip}


def load_signatures():
    sigs = {}
    for row in read_tsv(SIGNATURES):
        ins = [(reg, normalize_type(ctype))
               for item in row['inputs'].split(',') if ':' in item
               for reg, ctype in [item.split(':', 1)]]
        outs = [(reg, normalize_type(ctype))
                for item in row['outputs'].split(',') if ':' in item
                for reg, ctype in [item.split(':', 1)]]
        sigs[int(row['addr'], 16)] = (ins, outs)
    return sigs


def load_globals():
    return {int(row['addr'], 16): row for row in read_tsv(GLOBALS)}


def global_containing(address, globals_):
    """Return a reviewed scalar-sized global range containing ADDRESS."""
    for base, row in globals_.items():
        size_text = row['size'].strip()
        if not re.fullmatch(r'(?:0x[0-9a-fA-F]+|[0-9]+)', size_text):
            continue
        size = int(size_text, 0)
        if base <= address < base + size:
            return row
    return None


def dat_expression(address, globals_):
    row = globals_.get(address)
    ctype = normalize_type(row['type']) if row else ''
    raw = '0x%08x' % address
    access = {
        'uint8_t': 'EF_U8(%s)' % raw,
        'int8_t': '(*(int8_t *)ef_guest_memory_ptr(%s))' % raw,
        'uint16_t': 'EF_U16(%s)' % raw,
        'int16_t': 'EF_S16(%s)' % raw,
        'uint32_t': 'EF_U32(%s)' % raw,
        'ef_addr32': 'EF_U32(%s)' % raw,
        'int32_t': 'EF_S32(%s)' % raw,
        'float': '(*(float *)ef_guest_memory_ptr(%s))' % raw,
        'double': '(*(double *)ef_guest_memory_ptr(%s))' % raw,
        'uint64_t': '(*(uint64_t *)ef_guest_memory_ptr(%s))' % raw,
        'int64_t': '(*(int64_t *)ef_guest_memory_ptr(%s))' % raw,
    }
    return access.get(ctype, 'EF_U32(%s)' % raw)


# OpenWatcom pragma parameters name the containing register even for narrow
# C values; a char in EDX is read from DL and a short in EAX from AX.
REG32 = {'EAX': 'eax', 'EDX': 'edx', 'EBX': 'ebx', 'ECX': 'ecx', 'ESI': 'esi',
         'EDI': 'edi', 'AL': 'eax', 'AX': 'eax', 'DL': 'edx', 'DX': 'edx',
         'BL': 'ebx', 'BX': 'ebx', 'CL': 'ecx', 'CX': 'ecx'}
RETURN_REG = {'EAX': 'eax', 'EDX': 'edx', 'EBX': 'ebx', 'ECX': 'ecx',
              'ESI': 'esi', 'EDI': 'edi', 'AL': 'al', 'AX': 'ax',
              'DL': 'dl', 'DX': 'dx', 'BL': 'bl', 'BX': 'bx',
              'CL': 'cl', 'CX': 'cx'}


def pragma_for(name, sig):
    ins, outs = sig
    regs = [REG32.get(reg.upper()) for reg, _ in ins
            if not re.fullmatch(r'\[?ESP\+\d+\]?', reg.upper())]
    if any(reg is None for reg in regs) or len(outs) > 1:
        return None
    text = '#pragma aux %s parm [%s]' % (name, '] ['.join(regs)) if regs else \
           '#pragma aux %s parm caller []' % name
    if outs:
        out = RETURN_REG.get(outs[0][0].upper())
        if out is None:
            return None
        text += ' value [%s]' % out
    return text


def confirmed_alias_declarations(address, sig):
    """Declare a recovered guest callee under its generated name."""
    _, outputs = sig
    name = 'ef_fn_%08x' % address
    # A recovered callee can have no confirmed return in signatures.tsv while
    # Ghidra call sites still consume EAX. Keep those drafts compilable with
    # the compiler's ordinary integer return carrier; replay must validate it.
    result = outputs[0][1] if outputs else 'uint32_t'
    pragma = pragma_for(name, sig)
    declarations = [pragma] if pragma else []
    declarations.append('%s %s();' % (result, name))
    return declarations


def convert(text, entry, names, sig_in=None, globals_=None, sig_out=None):
    """Return (definition_signature, body_c, flags) for one pseudo-C file."""
    lines = text.split('\n')
    lines = [line for line in lines if not line.startswith(('/*', ' *'))]
    body = '\n'.join(lines).strip('\n')
    flags = set()
    extra = []
    inferred = []
    original_first = next((line for line in body.split('\n') if line.strip()), '')
    original = DEF.match(original_first)
    original_params = []
    original_param_types = []
    if original and original.group('params').strip() not in ('', 'void'):
        for parameter in original.group('params').split(','):
            parameter = parameter.strip()
            name = re.search(r'([A-Za-z_]\w*)\s*(?:\[[^]]*\])?\s*$', parameter)
            original_params.append(name.group(1) if name else None)
            original_param_types.append(
                normalize_type(parameter[:name.start()].strip()) if name else None)
    if sig_in:
        # Watcom register arguments: Ghidra shows them as in_<REG>.
        has_stack_inputs = any(
            re.fullmatch(r'\[?ESP\+\d+\]?', reg.upper()) for reg, _ in sig_in)
        generic_fastcall = bool(original and '__fastcall' in original_first)
        register_parameter_indices = {}
        for reg, ctype in sig_in:
            stack = re.fullmatch(r'\[?ESP\+(\d+)\]?', reg.upper())
            if stack:
                stack_index = max(0, (int(stack.group(1)) - 4) // 4)
                parameter_name = (original_params[stack_index]
                                  if stack_index < len(original_params) else None)
                if not parameter_name:
                    parameter_name = 'stack_%s' % stack.group(1)
                extra.append('%s %s' % (ctype if ctype else 'uint32_t', parameter_name))
                continue
            name = 'in_' + reg.upper()
            parameter_type = None
            if re.search(r'\b' + name + r'\b', body):
                declaration = re.search(
                    r'(?m)^\s*([A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*)\s*(\*+)?\s*' +
                    re.escape(name) + r'\s*;\s*$', body)
                if declaration:
                    parameter_type = (normalize_type(declaration.group(1)).strip() +
                                      ' ' + (declaration.group(2) or '')).strip()
                    body = body[:declaration.start()] + body[declaration.end():]
                body = re.sub(r'\b' + name + r'\b', 'r_' + reg.lower(), body)
            extra.append('%s r_%s' % (parameter_type or ctype or 'uint32_t', reg.lower()))
            register_parameter_indices[reg.upper()] = len(extra) - 1
        # Ghidra's generic __fastcall formals are not necessarily listed in
        # the same order as the confirmed Watcom register inputs. Prefer the
        # conventional ECX/EDX fastcall slots for the first two formals;
        # otherwise bind only a unique width-compatible confirmed input,
        # excluding registers already represented by in_/unaff_ live-ins.
        if generic_fastcall and not has_stack_inputs:
            def type_width(ctype):
                ctype = normalize_type(ctype or '').strip()
                if '*' in ctype:
                    return 32
                if ctype in ('char', 'signed char', 'unsigned char', 'int8_t', 'uint8_t'):
                    return 8
                if ctype in ('short', 'unsigned short', 'int16_t', 'uint16_t'):
                    return 16
                if ctype in ('double', 'int64_t', 'uint64_t', 'long long', 'unsigned long long'):
                    return 64
                if ctype in ('long double', 'float10'):
                    return 80
                return 32
            preferred = {0: 'ECX', 1: 'EDX'}
            function_body = body.split('\n', 1)[1] if '\n' in body else ''
            for parameter_index, original_name in enumerate(original_params):
                if (not original_name or not re.fullmatch(r'param_\d+', original_name) or
                        not re.search(r'\b' + re.escape(original_name) + r'\b', function_body)):
                    continue
                register = preferred.get(parameter_index)
                if (register not in register_parameter_indices or
                        re.search(r'\b(?:in|unaff)_' + register + r'\b', body)):
                    register = None
                if register is None:
                    candidates = []
                    for reg, ctype in sig_in:
                        reg = reg.upper()
                        if (reg not in register_parameter_indices or
                                re.search(r'\b(?:in|unaff)_' + reg + r'\b', body)):
                            continue
                        if type_width(original_param_types[parameter_index]) == type_width(ctype):
                            candidates.append(reg)
                    if len(candidates) == 1:
                        register = candidates[0]
                if register is None:
                    continue
                index = register_parameter_indices[register]
                extra[index] = '%s r_%s' % (
                    original_param_types[parameter_index] or sig_in[index][1],
                    register.lower())
                body = re.sub(r'\b' + re.escape(original_name) + r'\b',
                              'r_' + register.lower(), body)
        # Ghidra uses unaff_REG for values it could not associate with a
        # prototype.  Only lower one when the exact register is a confirmed
        # input and the body does not write the register itself.
        for register in sorted(set(re.findall(r'\bunaff_([A-Za-z0-9]+)\b', body))):
            register = register.upper()
            if register not in register_parameter_indices:
                continue
            name = 'unaff_' + register
            if re.search(r'(?m)^\s*' + name +
                         r'\s*(?:=(?!=)|\+\+|--|\+=|-=)', body):
                continue
            declaration = re.search(
                r'(?m)^\s*([A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*)\s*(\*+)?\s*' +
                re.escape(name) + r'\s*;\s*$', body)
            parameter_type = None
            if declaration:
                parameter_type = (normalize_type(declaration.group(1)).strip() +
                                  ' ' + (declaration.group(2) or '')).strip()
                if (declaration.group(2) and
                        re.search(r'\b' + name + r'\s*(?:<<|>>)', body)):
                    continue
                body = body[:declaration.start()] + body[declaration.end():]
            index = register_parameter_indices[register]
            extra[index] = '%s r_%s' % (
                parameter_type or sig_in[index][1] or 'uint32_t', register.lower())
            body = re.sub(r'\b' + name + r'\b', 'r_' + register.lower(), body)
    elif sig_in is None:
        # No confirmed signature: infer register inputs from in_<REG> uses.
        # HYPOTHESIS ONLY (plan rule 4); the pragma is marked inferred.
        fam = {'EAX': 'eax', 'AX': 'eax', 'AL': 'eax', 'AH': 'eax',
               'EDX': 'edx', 'DX': 'edx', 'DL': 'edx', 'DH': 'edx',
               'EBX': 'ebx', 'BX': 'ebx', 'BL': 'ebx', 'BH': 'ebx',
               'ECX': 'ecx', 'CX': 'ecx', 'CL': 'ecx', 'CH': 'ecx'}
        cast = {'E': 'uint32_t', 'X': 'uint16_t', 'L': 'uint8_t', 'H': 'uint8_t'}
        used = []
        inferred_types = {}
        for reg, base in fam.items():
            name = 'in_' + reg
            if re.search(r'\b' + name + r'\b', body):
                # Ghidra's declaration is only notation for a live-in
                # register. A write cannot be represented by an input-only
                # ABI parameter, so leave that function for manual handling.
                if re.search(r'\b' + name + r'\s*(?:=|\+\+|--|\+=|-=)', body):
                    flags.add('manual: write to live-in register ' + name)
                declaration = re.search(
                    r'(?m)^\s*([A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*)\s*(\*+)?\s*' +
                    re.escape(name) + r'\s*;\s*$', body)
                if declaration:
                    declared_type = normalize_type(declaration.group(1)).strip()
                    pointer_stars = declaration.group(2) or ''
                    inferred_types[base] = (declared_type + ' ' + pointer_stars).strip()
                body = re.sub(r'^\s*[A-Za-z_]\w*(?:\s+|\s*\*)+%s\s*;\s*$' % name,
                              '', body, flags=re.MULTILINE)
                body = re.sub(r'\b' + name + r'\b',
                              'r_' + base if reg.startswith('E') else
                              '((%s)r_%s)' % (cast[reg[-1]], base), body)
                if base not in used:
                    used.append(base)
        order = ['eax', 'edx', 'ebx', 'ecx']
        for base in sorted(used, key=order.index):
            extra.append(inferred_types.get(base, 'uint32_t') + ' r_' + base)
            inferred.append(base)
    # Materialise Ghidra's byte-offset field notation as an lvalue over the
    # original storage. This preserves overlapping local/register fragments
    # and writes; three-byte fields stay manual because they are not a C
    # scalar access.
    field_types = {1: 'uint8_t', 2: 'uint16_t', 4: 'uint32_t', 8: 'uint64_t'}
    def subfield_repl(match):
        base = match.group('base')
        offset = int(match.group('offset'))
        width = int(match.group('width'))
        ctype = field_types.get(width)
        if not ctype:
            flags.add('manual: unsupported %d-byte subfield' % width)
            return match.group(0)
        address = None
        if base.startswith(('DAT_', '_DAT_', 'PTR_')):
            address_match = re.search(r'([0-9a-fA-F]{8})$', base)
            if address_match:
                address = int(address_match.group(1), 16)
        elif base.startswith('s_'):
            address_match = re.search(r'_([0-9a-fA-F]{8})$', base)
            if address_match:
                address = int(address_match.group(1), 16)
        if address is not None:
            return '(*(%s *)ef_guest_memory_ptr(0x%08xu))' % (
                ctype, address + offset)
        return '(*(%s *)((char *)&%s + %d))' % (ctype, base, offset)
    body = re.sub(r'(?P<base>[A-Za-z_]\w*(?:\[[^\]]+\])*)\._'
                  r'(?P<offset>\d+)_(?P<width>\d+)_',
                  subfield_repl, body)
    # Ghidra's export carries pointer types in the expression uses rather
    # than in a separate global declaration.  An otherwise untyped DAT
    # global used as a dereference/index base is a pointer value stored at
    # that guest address, not an integer lvalue.  Preserve assignments to
    # the global itself, while translating read uses through the stored
    # 32-bit guest pointer.  Function-pointer globals need a callable cast;
    # their concrete signature is not available in the export.
    pointer_globals = set(re.findall(
        r'(?<![A-Za-z0-9_])_?DAT_([0-9a-fA-F]{8})(?![A-Za-z0-9_])(?=\s*(?:\[|->))', body))
    # A bare '*' before a DAT token is only a dereference when the preceding
    # non-space character cannot end an operand (otherwise it is multiply).
    for dereference in re.finditer(r'\*\s*_?DAT_([0-9a-fA-F]{8})\b', body):
        prefix = body[:dereference.start()].rstrip()
        previous = prefix[-1:] if prefix else ''
        if not previous or previous in '([=,:!~?&|+-*/%<>':
            pointer_globals.add(dereference.group(1))
    for address_text in sorted(pointer_globals):
        address = int(address_text, 16)
        # A known global declaration can have a wider or non-pointer type;
        # only infer a pointer for addresses absent from the reviewed table.
        if address in (globals_ or {}):
            continue
        token = re.compile(r'(?<![A-Za-z0-9_])_?DAT_' + address_text + r'\b')
        function_pointer = re.search(r'\(\s*\*\s*_?DAT_' + address_text + r'\s*\)\s*\(', body)
        cast = 'int (*)()' if function_pointer else 'char *'
        replacement = '((%s)(uintptr_t)EF_U32(0x%08xu))' % (cast, address)
        def pointer_read(match):
            end = match.end()
            suffix = body[end:]
            prefix = body[:match.start()]
            # Keep the guest global as an lvalue when the variable itself is
            # being assigned. A preceding unary '*' or following subscript
            # makes this a write through the stored pointer instead.
            if (re.match(r'\s*(?:=(?!=)|\+=|-=|\+\+|--)', suffix) and
                    not re.search(r'\*\s*$', prefix)):
                return 'EF_U32(0x%08xu)' % address
            return replacement
        body = token.sub(pointer_read, body)
    for token in {match.group(0) for match in MANUAL.finditer(body)}:
        flags.add('manual: ' + manual_cause(token))
    # A uRam label has no reliable width by itself. CONCAT22 does: the first
    # operand is specifically the high 16-bit half, so read that guest word
    # directly instead of treating it as an undeclared C identifier.
    body = re.sub(
        r'\bCONCAT22\(\s*uRam([0-9a-fA-F]{8})\s*,',
        lambda m: 'CONCAT22(EF_U16(0x%s),' % m.group(1).lower(), body)
    # Ghidra's CONCATxy(a, b) joins x bytes from a and y bytes from b.
    # Keep the operation unsigned and wide while shifting, then narrow to the
    # width Ghidra assigns to the combined value. Values wider than 64 bits
    # remain for manual handling.
    def concat_repl(match):
        high_bytes, low_bytes = map(int, match.groups())
        if high_bytes + low_bytes > 8:
            flags.add('manual: CONCAT wider than 8 bytes')
            return match.group(0)
        return 'EF_CONCAT_%d_%d(' % (high_bytes, low_bytes)
    body = re.sub(r'\bCONCAT([1-8])([1-8])\s*\(', concat_repl, body)
    for pattern, repl in TYPES:
        body = re.sub(pattern, repl, body)
    body = re.sub(r'\btrue\b', '1', body)
    body = re.sub(r'\bfalse\b', '0', body)
    globals_ = globals_ or {}
    # Ghidra sometimes assigns a function symbol to an overlaid immediate
    # operand when the original program modifies its own code. Treat a
    # non-call use as data only when a reviewed global starts at the byte
    # after the function entry (opcode at entry, writable immediate at +1).
    def function_overlay_repl(match):
        address = int(match.group(1), 16)
        row = globals_.get(address + 1)
        if row is None or re.match(r'\s*\(', body[match.end():]):
            return match.group(0)
        size_text = row['size'].strip()
        if (not re.fullmatch(r'(?:0x[0-9a-fA-F]+|[0-9]+)', size_text) or
                int(size_text, 0) < 4):
            return match.group(0)
        return 'EF_U32(0x%08xu)' % (address + 1)
    body = re.sub(r'\b(?:thunk_)?FUN_(' + HEX + r')\b',
                  function_overlay_repl, body)
    # iRam/uRam are Ghidra's signed/unsigned dword spellings for ambiguous
    # RAM symbols. Lower them only inside a reviewed global range so an
    # untyped byte alias (including self-modifying code) is not widened.
    def ram_int_repl(match):
        kind, address_text = match.groups()
        address = int(address_text, 16)
        if global_containing(address, globals_) is None:
            return match.group(0)
        access = 'EF_S32' if kind == 'iRam' else 'EF_U32'
        return '%s(0x%08xu)' % (access, address)
    body = re.sub(r'\b(iRam|uRam)([0-9a-fA-F]{8})\b', ram_int_repl, body)
    # Rename callees, referenced by auto name or by recovered name.
    body = re.sub(r'\b(?:thunk_)?FUN_' + HEX + r'\b',
                  lambda m: 'ef_fn_' + m.group(1).lower(), body)
    for old, new in names.items():
        body = re.sub(r'\b' + re.escape(old) + r'\b', new, body)
    # Ghidra emits ram0x names for referenced addresses that lack a data
    # symbol, and adds numeric suffixes for overlapping byte globals. The
    # latter are byte aliases at base+offset, as shown by their read/write
    # uses beside the corresponding DAT_<address> symbol.
    body = re.sub(r'(?<![A-Za-z0-9_])_?DAT_([0-9a-fA-F]{8})_(\d+)\b',
                  lambda m: 'EF_U8(0x%08xu)' %
                  (int(m.group(1), 16) + int(m.group(2))), body)
    body = re.sub(r'\bram0x([0-9a-fA-F]{8})\b',
                  lambda m: dat_expression(int(m.group(1), 16), globals_), body)
    body = re.sub(r'(?<![A-Za-z0-9_])_?DAT_' + HEX + r'\b',
                  lambda m: dat_expression(int(m.group(1), 16), globals_), body)
    # Ghidra's cRam/dRam labels retain useful scalar-width information but
    # do not use the DAT_ spelling handled above.  cRam is a byte access;
    # dRam is an IEEE double at the label address.  Keep the all-ones
    # cRam sentinel unresolved rather than turning it into an invalid guest
    # memory access.
    def ram_scalar_repl(match):
        kind, address_text = match.groups()
        address = int(address_text, 16)
        if kind == 'cRam':
            if address == 0xffffffff:
                return match.group(0)
            return 'EF_U8(0x%08xu)' % address
        return '(*(double *)ef_guest_memory_ptr(0x%08xu))' % address
    body = re.sub(r'\b(cRam|dRam)([0-9a-fA-F]{8})\b', ram_scalar_repl, body)
    # PTR_ names describe pointer-valued guest globals: dereference the slot
    # for reads/writes.  Taking &PTR then naturally yields the guest address
    # of that pointer slot, matching Ghidra's pointer-to-pointer expression.
    body = re.sub(r'\bPTR_\w*?_?' + HEX + r'\b',
                  lambda m: '(*(char **)ef_guest_memory_ptr(0x%s))' % m.group(1).lower(), body)
    body = re.sub(r'\b(?:s_[\w>]*?_|u_|BYTE_|WORD_|DWORD_)\w*?_?' + HEX + r'\b(?:\s*\+\s*\d+)?',
                  lambda m: '((char *)ef_guest_memory_ptr(0x%s))' % m.group(1).lower(), body)
    # An address-of a translated guest global is the guest address itself;
    # taking the C address of EF_U* would instead point into the host mapping
    # expression (and OpenWatcom does not treat that macro as addressable).
    guest_scalar_types = {'U8': 'uint8_t', 'U16': 'uint16_t', 'U32': 'uint32_t',
                          'S16': 'int16_t', 'S32': 'int32_t'}
    body = re.sub(
        r'&EF_(U8|U16|U32|S16|S32)\((0x[0-9a-fA-F]+)u?\)',
        lambda m: '((%s *)ef_guest_memory_ptr(%s))' %
        (guest_scalar_types[m.group(1)], m.group(2)), body)
    # PTR_ globals are pointer-valued slots.  Their address is a pointer to
    # that slot, which can be indexed or passed where Ghidra emitted &PTR.
    body = re.sub(
        r'&\(\(char \*\)ef_guest_memory_ptr\((0x[0-9a-fA-F]+)\)\)',
        r'((char **)ef_guest_memory_ptr(\1))', body)
    body = re.sub(r'\bLAB_' + HEX + r'\b', lambda m: 'lab_' + m.group(1).lower(), body)
    body = re.sub(r'\bfunc_0x' + HEX + r'\b', lambda m: 'ef_fn_' + m.group(1).lower(), body)
    # Ghidra emits an empty terminal case as a label immediately followed by
    # the switch's closing brace. C requires a statement after that label;
    # an empty statement preserves the original path (fall-through is only
    # preserved for labels followed by another case label).
    body = re.sub(r'(?m)^([ \t]*(?:case\b[^\n]*|default)[ \t]*:)[ \t]*(?=\n[ \t]*})',
                  r'\1\n;', body)
    first = next((line for line in body.split('\n') if line.strip()), '')
    match = DEF.match(first)
    if not match:
        flags.add('manual: no function definition recognized')
        return None, body, flags, inferred
    ret = ' '.join(match.group('ret').split()) or 'int'
    # Ghidra may model a narrow Watcom return register (AL/AX) using a wider
    # carrier type.  OpenWatcom validates `#pragma aux ... value [...]`
    # against the declared C return type, so use the confirmed ABI type as
    # the C carrier.  The implicit return conversion retains the low bits
    # that the caller observes.
    if sig_out and len(sig_out) == 1:
        ret = sig_out[0][1]
    params = match.group('params').strip() or 'void'
    if sig_in and extra:
        # Confirmed signatures replace Ghidra's generic prototype.
        params = ', '.join(extra)
    elif inferred:
        # Keep Ghidra's non-register parameters; place inferred register
        # inputs first so the pragma and the C declaration have the same ABI.
        params = ', '.join(extra + ([] if params == 'void' else [params]))
    sig = '%s ef_fn_%08x(%s)' % (ret, entry, params)
    rest = body.split('\n', 1)[1] if '\n' in body else ''
    return sig, rest, flags, inferred


def callees(body):
    return sorted(set(re.findall(r'\bef_fn_[0-9a-f]{8}\b', body)))


PREAMBLE = '''/* Mechanical draft derived from Ghidra output. NEVER COMMIT. */
#include "ef_abi.h"
#include "ef_mem.h"
#include "ef_gen_abi.h"
'''


def concat_macros():
    """Emit the finite CONCAT macros used by the current Ghidra export."""
    widths = range(1, 9)
    macros = []
    for high_bytes in widths:
        for low_bytes in widths:
            if high_bytes + low_bytes > 8:
                continue
            shift = low_bytes * 8
            high_mask = '0xffffffffffffffffULL' if high_bytes == 8 else \
                '0x%xULL' % ((1 << (high_bytes * 8)) - 1)
            low_mask = '0xffffffffffffffffULL' if low_bytes == 8 else \
                '0x%xULL' % ((1 << (low_bytes * 8)) - 1)
            result_type = 'uint32_t' if high_bytes + low_bytes <= 4 else 'uint64_t'
            expression = ('((((uint64_t)(a) & %s) << %d) | '
                          '((uint64_t)(b) & %s))' % (high_mask, shift, low_mask))
            macros.append('#define EF_CONCAT_%d_%d(a,b) ((%s)(%s))' %
                          (high_bytes, low_bytes, result_type, expression))
    return '\n'.join(macros)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--export', type=Path, default=EXPORT)
    parser.add_argument('--out', type=Path, default=OUT)
    parser.add_argument('--skip-recovered', action='store_true',
                        help='do not generate functions that already have hand-written bodies')
    parser.add_argument('--only', help='comma-separated hex addresses')
    parser.add_argument('--compile', action='store_true', help='compile with wcc386')
    parser.add_argument('--compiler', default=os.environ.get('WCC386', 'wcc386'))
    parser.add_argument('--limit', type=int)
    args = parser.parse_args()

    rows = read_tsv(args.export / 'functions.tsv')
    only = {int(a, 16) for a in args.only.split(',')} if args.only else None
    skip = recovered_addresses() if args.skip_recovered else set()
    sigs = load_signatures()
    globals_ = load_globals()
    names = {}
    for row in rows:
        entry = int(row['entry'], 16)
        if not re.fullmatch(r'(?:thunk_)?FUN_[0-9a-f]+', row['name']):
            names[row['name']] = 'ef_fn_%08x' % entry

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'src').mkdir(exist_ok=True)
    header, report = [], []
    if args.skip_recovered:
        # Recovered no-register-input and stack-only callees still need an
        # ABI alias for Ghidra's ef_fn_<address> spelling. Do not synthesize
        # aliases for register inputs: decompiler call arity may not describe
        # live-in regs.
        for entry in sorted(skip & sigs.keys()):
            inputs, _ = sigs[entry]
            if not inputs or all(re.fullmatch(r'\[?ESP\+\d+\]?', reg.upper())
                                 for reg, _ in inputs):
                header.extend(confirmed_alias_declarations(entry, sigs[entry]))
    count = 0
    for row in rows:
        entry = int(row['entry'], 16)
        if (only and entry not in only) or entry in skip:
            continue
        if args.limit is not None and count >= args.limit:
            break
        path = args.export / row['file']
        if not path.is_file():
            continue
        sig, body, flags, inferred = convert(
            path.read_text(errors='replace'), entry, names,
            sigs[entry][0] if entry in sigs else None, globals_,
            sigs[entry][1] if entry in sigs else None)
        state = 'manual' if flags else 'generated'
        if sig and entry in sigs:
            pragma = pragma_for('ef_fn_%08x' % entry, sigs[entry])
            if pragma:
                header.append(pragma)
        if sig and inferred and entry not in sigs:
            header.append('#pragma aux ef_fn_%08x parm [%s] /* inferred */' %
                          (entry, '] ['.join(inferred)))
        if sig:
            # Ghidra can assign different argument counts to the same callee
            # at different call sites, even when signatures.tsv confirms the
            # callee ABI. Keep the shared C declaration unspecified so each
            # generated call site compiles as Ghidra emitted it. The aux
            # pragma still maps explicit C arguments to the ABI registers;
            # replay remains responsible for finding missing live-register
            # arguments and other call-site mismatches.
            declaration = sig.split('(', 1)[0] + '()'
            header.append(declaration + ';')
        if state == 'generated':
            labels = sorted(set(re.findall(r'\blab_[0-9a-f]{8}\b', body)))
            (args.out / 'src' / ('ef_fn_%08x.c' % entry)).write_text(
                PREAMBLE + '\n' + sig + '\n' + body + '\n')
        report.append([row['entry'], row['name'], row['body_bytes'], state,
                       '; '.join(sorted(flags)) if state == 'manual' else ''])
        count += 1
    (args.out / 'ef_gen_abi.h').write_text(
        '/* Generated; derived from Ghidra output. NEVER COMMIT. */\n'
        '#include "ef_types.h"\n'
        # Ghidra FLOAT_ROUND is a floating-point operation, not an implicit
        # C int-returning call.  ROUND_ is supplied by the hybrid runtime.
        'double ROUND(double);\n'
        # Ghidra uses ``code *`` for heterogeneous callbacks.  An unspecified
        # argument list keeps its varied call sites compilable; int models a
        # common EAX return while still allowing callers to ignore it.
        'typedef int ef_gen_code();\n' + concat_macros() + '\n' +
        '\n'.join(dict.fromkeys(header)) + '\n')

    if args.compile:
        include = ['-i=' + str(ROOT / 'decomp/include'), '-i=' + str(args.out)]
        (args.out / 'obj').mkdir(exist_ok=True)
        for item in report:
            if item[3] != 'generated':
                continue
            source = args.out / 'src' / ('ef_fn_%08x.c' % int(item[0], 16))
            obj = args.out / 'obj' / (source.stem + '.obj')
            result = subprocess.run([args.compiler, *FLAGS, *include, '-fo=' + str(obj),
                                     str(source)], capture_output=True, text=True,
                                    cwd=args.out)
            if result.returncode == 0:
                item[3] = 'ok'
            else:
                item[3] = 'compile-error'
                lines = [line for line in (result.stdout + result.stderr).split('\n')
                         if 'Error' in line]
                item[4] = (lines[0] if lines else 'error')[:200].replace('\t', ' ')

    with open(args.out / 'report.tsv', 'w', newline='') as handle:
        writer = csv.writer(handle, delimiter='\t')
        writer.writerow(['addr', 'name', 'bytes', 'state', 'first_error'])
        writer.writerows(report)
    totals = {}
    for item in report:
        entry = totals.setdefault(item[3], [0, 0])
        entry[0] += 1
        entry[1] += int(item[2])
    for state, (n, size) in sorted(totals.items()):
        print('%-14s %5d functions %8d bytes' % (state, n, size))
    return 0


if __name__ == '__main__':
    sys.exit(main())
