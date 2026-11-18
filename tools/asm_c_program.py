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


def port_service_request(insn):
    """Decode only scalar IN/OUT encodings; never infer device behavior."""
    encoded = bytes.fromhex(insn['bytes'])
    word = encoded[:1] == b'\x66'
    body = encoded[1:] if word else encoded
    if not body or body[0] not in (0xe4, 0xe5, 0xe6, 0xe7, 0xec, 0xed, 0xee, 0xef):
        raise asm_to_c.Unsupported('unsupported port service encoding')
    opcode = body[0]
    immediate = opcode < 0xec
    if len(body) != (2 if immediate else 1) or (word and not opcode & 1):
        raise asm_to_c.Unsupported('unsupported port service encoding')
    width = (16 if word else 32) if opcode & 1 else 8
    direction = 'out' if opcode & 2 else 'in'
    accumulator = {8: 'al', 16: 'ax', 32: 'eax'}[width]
    port = ('0x%x' % body[1]) if immediate else 'dx'
    operands = [accumulator, port] if direction == 'in' else [port, accumulator]
    if insn['mnemonic'] != direction or insn['operands'].replace(' ', '').lower() != ','.join(operands):
        raise asm_to_c.Unsupported('port service encoding/operand mismatch')
    return dict(ip=insn['address'], direction=direction, width=width,
                port=body[1] if immediate else 'dx', length=len(encoded),
                status=0x1000 | (len(encoded) << 8) | opcode)


def interrupt_vector(insn):
    encoded = insn['bytes']
    if insn['mnemonic'] == 'int3' and encoded == 'cc':
        return 3
    if insn['mnemonic'] == 'int' and len(encoded) == 4 and encoded.startswith('cd'):
        return int(encoded[2:], 16)
    return None


def instruction_c(insn, starts, *, consuming_targets=frozenset(), interrupt_boundaries=False,
                  interrupt_sites=None, port_boundaries=False, port_sites=None, sti_stop_sites=(), cli_stop_sites=(), cpuid_sites=()):
    insn = legacy_transfer(insn)
    mnemonic = insn['mnemonic']
    if insn['address'] in sti_stop_sites:
        if mnemonic != 'sti' or insn['bytes'] != 'fb' or insn['operands']:
            raise asm_to_c.Unsupported('STI stop encoding/spelling mismatch')
        # Resume requires an external CPU implementation of STI and its interrupt shadow.
        return 'return 0x204u;'
    if insn['address'] in cli_stop_sites:
        if mnemonic != 'cli' or insn['bytes'] != 'fa' or insn['operands']:
            raise asm_to_c.Unsupported('CLI stop encoding/spelling mismatch')
        return 'return 0x206u;'
    if insn['address'] in cpuid_sites:
        if mnemonic != 'cpuid' or insn['bytes'] != '0fa2' or insn['operands']:
            raise asm_to_c.Unsupported('CPUID boundary encoding/spelling mismatch')
        next_pc = insn['address'] + 2
        if next_pc not in starts:
            raise asm_to_c.Unsupported('unresolved CPUID continuation')
        return 's->pc = 0x%xu; return 0x205u;' % next_pc
    vector = interrupt_vector(insn) if interrupt_boundaries else None
    if vector is not None and (insn['address'] in interrupt_sites if interrupt_sites is not None
                               else vector in (0x21, 0x31)):
        breakpoint = mnemonic == 'int3'
        next_pc = insn['address'] + (1 if breakpoint else 2)
        if not breakpoint and next_pc not in starts:
            raise asm_to_c.Unsupported('unresolved interrupt return continuation')
        # A host-service boundary, not architectural interrupt delivery.
        # No stack frame, flag change or service result is fabricated.
        return 's->pc = 0x%xu; return 0x%xu;' % (next_pc, 0x203 if breakpoint else 0x100 + vector)
    if (port_boundaries and mnemonic in ('in', 'out') and
            (port_sites is None or insn['address'] in port_sites)):
        request = port_service_request(insn)
        next_pc = insn['address'] + request['length']
        if next_pc not in starts:
            raise asm_to_c.Unsupported('unresolved port return continuation')
        return 's->pc = 0x%xu; return 0x%xu;' % (next_pc, request['status'])
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


def analyse(records, *, interrupt_boundaries=False, interrupt_sites=None,
            port_boundaries=False, port_sites=None, failure_details=None, sti_stop_sites=(), cli_stop_sites=(), cpuid_sites=()):
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
                       interrupt_boundaries=interrupt_boundaries, interrupt_sites=interrupt_sites,
                       port_boundaries=port_boundaries, port_sites=port_sites,
                       sti_stop_sites=sti_stop_sites, cli_stop_sites=cli_stop_sites, cpuid_sites=cpuid_sites)
    codes, dependencies, reasons = {}, {}, {}
    for record in records:
        address = record['address']
        try:
            codes[address] = asm_to_c.translate(record, starts=starts, renderer=renderer)
        except asm_to_c.Unsupported as error:
            reasons[address] = str(error)
            if failure_details is not None:
                # Only audit established bodies. Layout rejection is a body
                # failure, not evidence about individual instructions.
                established = (record.get('boundary') == 'ghidra-body-ranges' and
                               record.get('ghidra_instruction_layout_matches') is True and
                               not record.get('overlapping_entries') and
                               not record.get('undecodable_addresses') and
                               address in {i['address'] for i in record['instructions']})
                if established:
                    failures = []
                    for insn in record['instructions']:
                        try:
                            renderer(insn, starts)
                        except asm_to_c.Unsupported as instruction_error:
                            failures.append(dict(address=insn['address'],
                                                 mnemonic=insn['mnemonic'],
                                                 reason=str(instruction_error)))
                    failure_details[address] = failures
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
/* Callback arguments: state, original instruction IP, request, context.
 * INT requests are vectors from the manifest. Port requests encode
 * 0x1000 | (instruction_length << 8) | opcode. Decode the original bytes
 * to distinguish operand-size prefixes and immediate ports; read DX low word
 * for register ports. IN must update only AL/AX/EAX; OUT preserves registers.
 * These are host policy boundaries, without architectural I/O privilege checks.
 */
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
        if (((status >= 0x100u && status <= 0x1ffu) || status == 0x203u) && service) {
            unsigned length = status == 0x203u ? 1u : 2u;
            status = service(s, s->pc - length, status & 0xffu, context);
            if (status) return status;
            if (s->fault) return s->fault;
            status = s->pc == stop_pc ? 0 : 1;
        }
        if ((status & 0xf000u) == 0x1000u && service) {
            unsigned length = (status >> 8) & 15u;
            status = service(s, s->pc - length, status, context);
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

def reachable_service_sites(records, dependencies, closed, *, ports=False):
    """Conservative direct-call closure; request numbers and indirect edges are unknown."""
    sites = {r['address']: {i['address'] for i in r['instructions']
                           if ((i['mnemonic'] in ('in', 'out')) if ports else
                               (interrupt_vector(i) is not None))}
             for r in records if r['address'] in closed}
    callers = {address: set() for address in closed}
    for address in closed:
        for target in dependencies[address] & closed:
            callers[target].add(address)
    pending = {address for address, requests in sites.items() if requests}
    while pending:
        target = pending.pop()
        for caller in callers[target]:
            added = sites[target] - sites[caller]
            if added:
                sites[caller].update(added)
                pending.add(caller)
    return {address: sorted(requests) for address, requests in sites.items()}


def generate(inventory, output, image, *, shard_entries=2048, interrupt_boundaries=False,
             interrupt_sites=None, port_boundaries=False, port_sites=None, sti_stop_sites=(), cli_stop_sites=(), cpuid_sites=(),
             allow_partial_closure=False):
    if port_sites is not None and not port_boundaries:
        raise ValueError('port sites require port boundaries')
    if interrupt_sites is not None and not interrupt_boundaries:
        raise ValueError('interrupt sites require interrupt boundaries')
    manifest, records = asm_to_c.read_inventory(inventory, image)
    sti_stop_sites, cli_stop_sites = map(frozenset, (sti_stop_sites, cli_stop_sites))
    for sites, mnemonic, encoding in ((sti_stop_sites, 'sti', 'fb'), (cli_stop_sites, 'cli', 'fa')):
        if sites:
            valid = {i['address'] for r in records if r['ghidra_instruction_layout_matches'] is True
                     for i in r['instructions'] if i['mnemonic'] == mnemonic and i['bytes'] == encoding
                     and not i['operands']}
            if sites - valid:
                raise ValueError(f'{mnemonic.upper()} stop sites must be reviewed {mnemonic.upper()} instruction starts')
    cpuid_sites = frozenset(cpuid_sites)
    if cpuid_sites:
        valid_cpuid = {i['address'] for r in records if r['ghidra_instruction_layout_matches'] is True
                       for i in r['instructions'] if i['mnemonic'] == 'cpuid'
                       and i['bytes'] == '0fa2' and not i['operands']}
        if cpuid_sites - valid_cpuid:
            raise ValueError('CPUID sites must be reviewed CPUID instruction starts')
    failure_details = {}
    codes, dependencies, closed, reasons = analyse(records, failure_details=failure_details,
                                                  interrupt_boundaries=interrupt_boundaries,
                                                  interrupt_sites=interrupt_sites,
                                                  port_boundaries=port_boundaries, port_sites=port_sites,
                                                  sti_stop_sites=sti_stop_sites, cli_stop_sites=cli_stop_sites, cpuid_sites=cpuid_sites)
    blockers = root_blockers(dependencies, closed, reasons)
    emitted = set(codes) if allow_partial_closure else closed
    output.mkdir(parents=True, exist_ok=False)
    (output / 'ef_lift_state.h').write_text(asm_to_c.RUNTIME)
    service_sites = reachable_service_sites(records, dependencies, emitted)
    port_service_sites = reachable_service_sites(records, dependencies, emitted, ports=True)
    report = []
    for record in records:
        address = record['address']
        item = dict(address=address, state='unsupported', reason=reasons.get(address, ''))
        if address not in closed:
            item['root_blockers'] = blockers[address]
            if address in failure_details:
                item['instruction_failures'] = failure_details[address]
            if address in dependencies:
                item['dependencies'] = sorted(dependencies[address])
        if address in emitted:
            item.pop('reason')
            if address not in closed:
                item['incomplete_direct_closure'] = True
            item.update(reachable_host_service_sites=service_sites[address],
                        reachable_host_port_service_sites=port_service_sites[address],
                        state='generated-unverified', file='ef_lift_%08x.c' % address,
                        source_sha256=hashlib.sha256(codes[address].encode()).hexdigest(),
                        dependencies=sorted(dependencies[address]),
                        dynamic_transfer_sites=[i['address'] for i in map(legacy_transfer, record['instructions'])
                                                if i['mnemonic'] in ('call', 'jmp') and
                                                'direct_target' not in i],
                        instruction_addresses=[i['address'] for i in record['instructions']],
                        body_bytes=sum(end-start for start, end in record['ranges']))
            (output / item['file']).write_text(codes[address])
        report.append(item)
    files = dispatcher([r for r in records if r['address'] in emitted], shard_entries=shard_entries)
    for name, code in files.items():
        (output / name).write_text(code)
    program = files['ef_lift_program.c']
    result = dict(schema=1, model=MODEL, payload_sha256=manifest['payload_sha256'],
                  state_abi=asm_to_c.STATE_ABI,
                  inventory_sha256=hashlib.sha256((inventory / 'manifest.json').read_bytes()).hexdigest(),
                  runtime_sha256=hashlib.sha256(asm_to_c.RUNTIME.encode()).hexdigest(),
                  dispatcher=dict(file='ef_lift_program.c', source_sha256=hashlib.sha256(program.encode()).hexdigest()),
                  syntactically_supported=len(codes), entries=report)
    if 'missing_body_addresses' in manifest:
        result['missing_body_addresses'] = manifest['missing_body_addresses']
    impact = Counter(root for roots in blockers.values() for root in roots)
    result['root_blocker_impact'] = [
        dict(address=root, affected_entries=count, reason=reasons[root])
        for root, count in sorted(impact.items(), key=lambda item: (-item[1], item[0]))
    ]
    result['root_blocker_impact_scope'] = (
        'direct dependencies only; counts include each root itself and can overlap')
    result['dispatcher_layout'] = 'pc-shards-v1'
    result['dispatcher_shard_entries'] = shard_entries
    result['dispatcher_support'] = [dict(file=name, source_sha256=hashlib.sha256(code.encode()).hexdigest())
                                    for name, code in sorted(files.items()) if name != 'ef_lift_program.c']
    result['closure_scope'] = 'direct-edges-only; dynamic targets require runtime coverage'
    if allow_partial_closure:
        result['closure_scope'] = 'partial-direct-closure; dispatcher stops at untranslated PCs'
    result['instruction_failure_scope'] = (
        'all renderer rejections in established root bodies; body-layout failures excluded; '
        'not an execution, equivalence or indirect-target coverage proof')
    result['host_service_reachability_scope'] = (
        'interrupt sites only; conservative direct-edge closure; dynamic targets and runtime request numbers unknown; '
        'reachable sites do not prove service implementation or equivalence')
    result['host_port_service_reachability_scope'] = (
        'scalar port sites only; conservative direct-edge closure; dynamic targets and '
        'runtime DX port numbers unknown; reachable sites do not prove device implementation or equivalence')
    if cpuid_sites:
        result['cpuid_sites'] = sorted(cpuid_sites)
    if cli_stop_sites:
        result['cli_stop_sites'] = sorted(cli_stop_sites)
    if sti_stop_sites:
        result['sti_stop_sites'] = sorted(sti_stop_sites)
    if interrupt_boundaries:
        sites = sorted((i['address'], interrupt_vector(i), i['mnemonic'] == 'int3')
                       for r in records if r['address'] in emitted
                       for i in r['instructions'] if interrupt_vector(i) is not None)
        result['host_service_boundaries'] = dict(
            model='int-service-yield-v2',
            statuses={f'0x{vector:02x}': 0x100 + vector for vector in sorted({v for _, v, trap in sites if not trap})},
            sites=[ip for ip, _, _ in sites],
            requests=[dict(ip=ip, vector=vector, length=1, status=0x203) if trap
                      else dict(ip=ip, vector=vector) for ip, vector, trap in sites],
            contract='PC is continuation; service IP is PC-encoding length; vector is low status byte; '
                     'host must service request before resuming; INT3 may lack a decoded continuation')
    if port_boundaries:
        result['host_port_service_boundaries'] = dict(
            model='scalar-port-service-yield-v1',
            requests=[port_service_request(i) for r in records if r['address'] in emitted
                      for i in r['instructions'] if i['mnemonic'] in ('in', 'out')],
            contract='PC is continuation; callback IP is PC-length; request is yield status; '
                     'host must implement IN accumulator update or OUT device effect; '
                     'no architectural privilege checks or device behavior are implemented')
    asm_to_c.bind_backends(result, [codes[a] for a in sorted(emitted)])
    (output / 'report.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, default=asm_to_c.ROOT / 'local/ghidra')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--interrupt-boundaries', action='store_true',
                        help='yield INT 21h/31h or explicitly selected INT sites to an external host service')
    parser.add_argument('--interrupt-site', type=lambda value: int(value, 0), action='append',
                        help='yield only these exact INT or INT3 instruction addresses (repeatable)')
    parser.add_argument('--port-boundaries', action='store_true',
                        help='yield scalar IN/OUT to a host service; does not implement devices')
    parser.add_argument('--port-site', type=lambda value: int(value, 0), action='append',
                        help='restrict yielded IN/OUT instruction addresses (repeatable)')
    parser.add_argument('--cpuid-site', type=lambda value: int(value, 0), action='append', default=[],
                        help='yield at a reviewed CPUID instruction for CPU-model emulation')
    parser.add_argument('--sti-stop-site', type=lambda value: int(value, 0), action='append', default=[],
                        help='stop before an explicit STI, leaving PC and flags unchanged (repeatable)')
    parser.add_argument('--cli-stop-site', type=lambda value: int(value, 0), action='append', default=[],
                        help='stop before an explicit CLI, leaving PC and flags unchanged (repeatable)')
    parser.add_argument('--allow-partial-closure', action='store_true',
                        help='emit supported bodies even when some direct dependencies are untranslated')
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if not output.is_relative_to(asm_to_c.ROOT / 'local'):
            raise ValueError('generated ASM-derived C must stay under ignored local/')
        workspace = asm_to_c.asm_inventory.ghidra_ef2000.validated_workspace(args.workspace)
        report = generate(args.inventory, output, workspace / 'payload.bin',
                          interrupt_boundaries=args.interrupt_boundaries,
                          interrupt_sites=None if args.interrupt_site is None else frozenset(args.interrupt_site),
                          port_boundaries=args.port_boundaries,
                          port_sites=None if args.port_site is None else frozenset(args.port_site),
                          sti_stop_sites=args.sti_stop_site, cli_stop_sites=args.cli_stop_site, cpuid_sites=args.cpuid_site,
                          allow_partial_closure=args.allow_partial_closure)
        print(dict(Counter(e['state'] for e in report['entries'])))
    except (OSError, ValueError) as error:
        parser.exit(1, f'asm_c_program: {error}\n')


if __name__ == '__main__':
    main()
