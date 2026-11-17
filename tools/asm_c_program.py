#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

"""Compose checked ASM translations without an original-code fallback.

Only bodies closed over translated direct dependencies are emitted. The shared
PC dispatcher executes one guest instruction at a time on the same guest RAM
and stack, so recursion, tail jumps and return addresses do not depend on host
C stack layout or guessed ABI signatures. Indirect targets are checked against
the translated PC registry at runtime; direct closure is not an indirect-target
coverage proof.
"""

import argparse
from collections import Counter
import hashlib
import json
from functools import partial
from pathlib import Path

try:
    from . import asm_to_c
except ImportError:
    import asm_to_c


MODEL = 'flat-x86-program-explicit-state'


def legacy_transfer(insn):
    """Interpret objdump's CET-era alias in the pre-CET 32-bit model."""
    if insn['mnemonic'] != 'notrack':
        return insn
    encoded = bytes.fromhex(insn['bytes'])
    parts = insn['operands'].split(None, 1)
    if (len(encoded) < 3 or encoded[:2] != b'\x3e\xff' or
            (encoded[2] >> 3) & 7 != 4 or encoded[2] >> 6 == 3 or
            len(parts) != 2 or parts[0] != 'jmp'):
        raise asm_to_c.Unsupported('unsupported legacy NOTRACK encoding/spelling')
    target = asm_to_c.operand(parts[1])
    if target[0] != 'mem' or asm_to_c.width_of(target) != 32:
        raise asm_to_c.Unsupported('unsupported legacy NOTRACK operand')
    # The prefix overrides even an EBP/ESP base's default SS selection.
    text = parts[1]
    if ':' in text:
        raise asm_to_c.Unsupported('ambiguous legacy NOTRACK segment')
    text = text.replace('[', 'ds:[', 1)
    return dict(insn, mnemonic='jmp', operands=text)


def instruction_c(insn, starts, *, consuming_targets=frozenset(), interrupt_boundaries=False,
                  interrupt_sites=None):
    insn = legacy_transfer(insn)
    mnemonic = insn['mnemonic']
    if (interrupt_boundaries and mnemonic == 'int' and insn['bytes'] in ('cd21', 'cd31') and
            (interrupt_sites is None or insn['address'] in interrupt_sites)):
        next_pc = insn['address'] + 2
        if next_pc not in starts:
            raise asm_to_c.Unsupported('unresolved interrupt return continuation')
        # A host-service boundary, not architectural interrupt delivery.
        # No stack frame, flag change or service result is fabricated.
        return 's->pc = 0x%xu; return 0x%xu;' % (next_pc, 0x100 + int(insn['bytes'][2:], 16))
    indirect = mnemonic in ('call', 'jmp') and 'direct_target' not in insn
    if mnemonic != 'call' and not indirect:
        return asm_to_c.instruction_c(insn, starts)
    next_pc = insn['address'] + len(bytes.fromhex(insn['bytes']))
    if (mnemonic == 'call' and next_pc not in starts and
            insn.get('direct_target') not in consuming_targets):
        raise asm_to_c.Unsupported('unresolved direct call or return continuation')
    if indirect:
        target = asm_to_c.operand(insn['operands'])
        if target[0] not in ('reg', 'mem') or asm_to_c.width_of(target) != 32:
            raise asm_to_c.Unsupported('unsupported indirect transfer operand')
        # CALL [ESP-4], [ESP] and ESP must fetch their target before the
        # return-PC push changes either ESP or the target's memory slot.
        code = ['a = %s; if (s->fault) return s->fault;' % asm_to_c.read(target, 32)]
        target_expression = 'a'
    else:
        target = insn['direct_target']
        if target not in starts:
            raise asm_to_c.Unsupported('unresolved direct call or return continuation')
        code = []
        target_expression = '0x%xu' % target
    if mnemonic == 'call':
        code += ['if (ef_check_segment(s, 1, %s, 1, 2)) return s->fault;' % target_expression,
                 'ef_write_segment(s, 2, s->r[4] - 4u, 32, 0x%xu);' % next_pc,
                 'if (s->fault) return s->fault; s->r[4] -= 4u;']
    code.append('s->pc = %s; break;' % target_expression)
    return ' '.join(code)


def analyse(records, *, interrupt_boundaries=False, interrupt_sites=None):
    owners = {}
    for record in records:
        if record.get('ghidra_instruction_layout_matches') is not True:
            continue
        for insn in record['instructions']:
            if insn['address'] in owners:
                raise ValueError('overlapping instruction ownership')
            owners[insn['address']] = record['address']
    starts = set(owners)
    # A canonical POP r32; RET consumes the nested CALL continuation as data
    # and returns through the caller's outer stack slot. Require both exact
    # encodings and instruction ownership; never guess this from a symbol or
    # a Ghidra flow override. ESP is excluded because POP ESP replaces the
    # stack pointer rather than simply consuming the continuation.
    consuming_targets = set()
    for record in records:
        instructions = record['instructions']
        if len(instructions) != 2:
            continue
        pop, ret = instructions
        encoded = bytes.fromhex(pop['bytes'])
        if (pop['address'] == record['address'] and
                owners.get(pop['address']) == record['address'] and
                owners.get(ret['address']) == record['address'] and
                len(encoded) == 1 and 0x58 <= encoded[0] <= 0x5f and
                encoded[0] != 0x5c and pop['mnemonic'] == 'pop' and
                pop['operands'].lower() == asm_to_c.REGS[encoded[0]-0x58] and
                ret['address'] == pop['address'] + 1 and
                ret['mnemonic'] == 'ret' and ret['bytes'] == 'c3' and
                not ret['operands']):
            consuming_targets.add(record['address'])
    renderer = partial(instruction_c, consuming_targets=consuming_targets,
                       interrupt_boundaries=interrupt_boundaries, interrupt_sites=interrupt_sites)
    codes, dependencies, reasons = {}, {}, {}
    for record in records:
        address = record['address']
        try:
            codes[address] = asm_to_c.translate(record, starts=starts, renderer=renderer)
        except asm_to_c.Unsupported as error:
            reasons[address] = str(error)
            continue
        edges = asm_to_c.asm_inventory.control_flow_report(
            [legacy_transfer(i) for i in record['instructions']])['edges']
        dependencies[address] = {owners[edge['target']] for edge in edges
                                 if edge['kind'] in ('call', 'jump', 'fallthrough') and
                                 edge['target'] in owners and owners[edge['target']] != address}
    closed = set(codes)
    while True:
        reduced = {address for address in closed if dependencies[address] <= closed}
        if reduced == closed:
            break
        closed = reduced
    for address in set(codes) - closed:
        missing = sorted(dependencies[address] - closed)
        reasons[address] = 'untranslated direct dependencies: ' + ', '.join(hex(a) for a in missing)
    return codes, dependencies, closed, reasons


def root_blockers(dependencies, closed, reasons):
    """Propagate first translation failures through direct edges, including cycles.

    These are observed blockers, not an exhaustive instruction audit: fixing a
    body's first failure can expose another failure in that same body.
    """
    roots = {address: ({address} if address not in dependencies else set())
             for address in reasons}
    changed = True
    while changed:
        changed = False
        for address, targets in dependencies.items():
            if address in closed:
                continue
            found = roots[address]
            before = len(found)
            for target in targets - closed:
                found.update(roots[target])
            changed |= len(found) != before
    return {address: sorted(targets) for address, targets in roots.items()}


def dispatcher(records, *, shard_entries=2048):
    """Keep each Watcom unit below its constant-initializer limits."""
    if type(shard_entries) is not int or not 1 <= shard_entries <= 2048:
        raise ValueError('registry shard size must be between 1 and 2048')
    prototypes = ['unsigned ef_lift_%08x(ef_lift_state *, uint32_t);' % r['address']
                  for r in records]
    locations = sorted((i['address'], r['address'], bytes.fromhex(i['bytes']),
                        asm_to_c.mutable_immediate(i))
                       for r in records for i in r['instructions'])
    if not locations:
        raise ValueError('no closed translated instructions')
    header = '''/* Generated ASM-derived registry declarations. NEVER COMMIT. */
#ifndef EF_LIFT_REGISTRY_H
#define EF_LIFT_REGISTRY_H
#include "ef_lift_state.h"
typedef unsigned (*ef_lift_step_function)(ef_lift_state *, uint32_t);
typedef unsigned (*ef_lift_service_function)(ef_lift_state *, uint32_t, unsigned, void *);
unsigned ef_lift_run(ef_lift_state *, uint32_t, uint32_t);
unsigned ef_lift_run_services(ef_lift_state *, uint32_t, uint32_t,
                              ef_lift_service_function, void *);
typedef struct {
    uint32_t pc; ef_lift_step_function step;
    uint32_t code_offset; uint8_t length;
} ef_lift_location;
'''+ '\n'.join(prototypes)+'\n'
    files, chunks = {}, []
    for number, start in enumerate(range(0, len(locations), shard_entries)):
        entries, original = [], bytearray()
        group = locations[start:start+shard_entries]
        for pc, address, encoded, immediate in group:
            if not 1 <= len(encoded) <= 15:
                raise ValueError('invalid x86 instruction length')
            entries.append('    {0x%xu, ef_lift_%08x, %du, %du},' %
                           (pc, address, len(original), immediate[0] if immediate else len(encoded)))
            original.extend(encoded)
        name, code = 'ef_locations_%04d'%number, 'ef_code_%04d'%number
        header += 'extern const ef_lift_location %s[%d];\n' % (name,len(group))
        header += 'extern const uint8_t %s[%d];\n' % (code,len(original))
        byte_lines = ['    '+','.join('0x%02x'%b for b in original[i:i+16])+','
                      for i in range(0,len(original),16)]
        files['ef_lift_registry_%04d.c'%number] = (
            '/* Generated ASM-derived PC/byte table. NEVER COMMIT. */\n'
            '#include "ef_lift_registry.h"\nconst ef_lift_location %s[] = {\n%s\n};\n'
            'const uint8_t %s[] = {\n%s\n};\n' % (name,'\n'.join(entries),code,'\n'.join(byte_lines)))
        chunks.append('    {0x%xu, %du, %s, %s},' % (group[0][0],len(group),name,code))
    files['ef_lift_registry.h'] = header+'#endif\n'
    files['ef_lift_program.c'] = '''/* Generated ASM-derived program dispatcher. NEVER COMMIT. */
#include "ef_lift_registry.h"
typedef struct {
    uint32_t first_pc, count;
    const ef_lift_location *locations;
    const uint8_t *code;
} ef_lift_chunk;
static const ef_lift_chunk ef_chunks[] = {
%s
};
unsigned ef_lift_run(ef_lift_state *s, uint32_t fuel, uint32_t stop_pc) {
    unsigned status;
    uint32_t low, high, middle, byte_index, code_byte;
    const ef_lift_chunk *chunk;
    const ef_lift_location *location;
    if (s->fault) return s->fault;
    while (fuel--) {
        if (s->pc == stop_pc) return 0;
        low = 0; high = sizeof(ef_chunks) / sizeof(ef_chunks[0]);
        while (low < high) {
            middle = low + (high-low)/2;
            if (ef_chunks[middle].first_pc <= s->pc) low = middle + 1;
            else high = middle;
        }
        if (!low) return 2;
        chunk = &ef_chunks[low-1];
        low = 0; high = chunk->count;
        while (low < high) {
            middle = low + (high-low)/2;
            if (chunk->locations[middle].pc < s->pc) low = middle + 1;
            else high = middle;
        }
        if (low == chunk->count || chunk->locations[low].pc != s->pc) return 2;
        location = &chunk->locations[low];
        for (byte_index = 0; byte_index < location->length; ++byte_index) {
            code_byte = ef_read(s, s->pc + byte_index, 8);
            if (s->fault) return s->fault;
            if (code_byte != chunk->code[location->code_offset + byte_index]) return 6;
        }
        status = location->step(s, 1);
        /* RET and ordinary completion each consume exactly one shared unit. */
        if (status != 0 && status != 1) return status;
    }
    return s->pc == stop_pc ? 0 : 1;
}
unsigned ef_lift_run_services(ef_lift_state *s, uint32_t fuel, uint32_t stop_pc,
                              ef_lift_service_function service, void *context) {
    unsigned status;
    if (s->fault) return s->fault;
    while (fuel--) {
        status = ef_lift_run(s, 1, stop_pc);
        if ((status == 0x121u || status == 0x131u) && service) {
            status = service(s, s->pc - 2u, status - 0x100u, context);
            if (status) return status;
            if (s->fault) return s->fault;
            status = s->pc == stop_pc ? 0 : 1;
        }
        if (status != 1) return status;
    }
    return s->pc == stop_pc ? 0 : 1;
}
''' % '\n'.join(chunks)
    return files

def generate(inventory, output, image, *, shard_entries=2048, interrupt_boundaries=False,
             interrupt_sites=None):
    if interrupt_sites is not None and not interrupt_boundaries:
        raise ValueError('interrupt sites require interrupt boundaries')
    manifest, records = asm_to_c.read_inventory(inventory, image)
    codes, dependencies, closed, reasons = analyse(records, interrupt_boundaries=interrupt_boundaries,
                                                  interrupt_sites=interrupt_sites)
    blockers = root_blockers(dependencies, closed, reasons)
    output.mkdir(parents=True, exist_ok=False)
    (output / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
    report = []
    for record in records:
        address = record['address']
        item = dict(address=address, state='unsupported', reason=reasons.get(address, ''))
        if address not in closed:
            item['root_blockers'] = blockers[address]
            if address in dependencies:
                item['dependencies'] = sorted(dependencies[address])
        if address in closed:
            item.update(state='generated-unverified', file='ef_lift_%08x.c' % address,
                        source_sha256=hashlib.sha256(codes[address].encode()).hexdigest(),
                        dependencies=sorted(dependencies[address]),
                        dynamic_transfer_sites=[i['address'] for i in map(legacy_transfer, record['instructions'])
                                                if i['mnemonic'] in ('call', 'jmp') and
                                                'direct_target' not in i],
                        instruction_addresses=[i['address'] for i in record['instructions']],
                        body_bytes=sum(end-start for start, end in record['ranges']))
            (output / item['file']).write_text(codes[address])
        report.append(item)
    files = dispatcher([r for r in records if r['address'] in closed], shard_entries=shard_entries)
    for name, code in files.items():
        (output / name).write_text(code)
    program = files['ef_lift_program.c']
    result = dict(schema=1, model=MODEL, payload_sha256=manifest['payload_sha256'],
                  state_abi=asm_to_c.STATE_ABI,
                  inventory_sha256=hashlib.sha256((inventory / 'manifest.json').read_bytes()).hexdigest(),
                  runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                  dispatcher=dict(file='ef_lift_program.c', source_sha256=hashlib.sha256(program.encode()).hexdigest()),
                  syntactically_supported=len(codes), entries=report)
    result['dispatcher_layout'] = 'pc-shards-v1'
    result['dispatcher_shard_entries'] = shard_entries
    result['dispatcher_support'] = [dict(file=name, source_sha256=hashlib.sha256(code.encode()).hexdigest())
                                    for name, code in sorted(files.items()) if name != 'ef_lift_program.c']
    result['closure_scope'] = 'direct-edges-only; dynamic targets require runtime coverage'
    if interrupt_boundaries:
        sites = sorted((i['address'], int(i['bytes'][2:], 16))
                       for r in records if r['address'] in closed
                       for i in r['instructions'] if i['mnemonic'] == 'int'
                       and i['bytes'] in ('cd21', 'cd31'))
        result['host_service_boundaries'] = dict(
            model='int-service-yield-v2',
            statuses={'0x21': 0x121, '0x31': 0x131},
            sites=[ip for ip, vector in sites],
            requests=[dict(ip=ip, vector=vector) for ip, vector in sites],
            contract='PC is continuation; service IP is PC-2; vector is status-0x100; host must service request before resuming')
    asm_to_c.bind_backends(result, [codes[a] for a in sorted(closed)])
    (output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, default=asm_to_c.ROOT / 'local/ghidra')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--interrupt-boundaries', action='store_true',
                        help='yield INT 21h/31h to an external host service; does not implement the service')
    parser.add_argument('--interrupt-site', type=lambda value: int(value, 0), action='append',
                        help='restrict yielded INT 21h/31h sites to these instruction addresses (repeatable)')
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to(asm_to_c.ROOT / 'local'):
            raise ValueError('generated ASM-derived C must stay under ignored local/')
        workspace = asm_to_c.asm_inventory.ghidra_ef2000.validated_workspace(args.workspace)
        report = generate(args.inventory, output, workspace / 'payload.bin',
                          interrupt_boundaries=args.interrupt_boundaries,
                          interrupt_sites=None if args.interrupt_site is None else frozenset(args.interrupt_site))
        print(dict(Counter(e['state'] for e in report['entries'])))
    except (OSError, ValueError) as error:
        parser.exit(1, f'asm_c_program: {error}\n')


if __name__ == '__main__':
    main()
