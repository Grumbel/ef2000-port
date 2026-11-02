// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** Recover aligned direct-CALL destinations, without interpreting arbitrary data as code. */
public class EF2000RecoverCalls extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;
    private static final long CODE_END = 0x110000L;
    private static final int MAX_DISASSEMBLY_BYTES = 1024;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 4 || (!args[1].equals("report") && !args[1].equals("apply")) ||
                (!args[3].equals("padding") && !args[3].equals("adjacent-ret"))) {
            throw new IllegalArgumentException(
                "expected report.tsv, report|apply, maximum (0=all), padding|adjacent-ret");
        }
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(0)) == null ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected zero-based EF2000 X-32 image");
        }
        boolean apply = args[1].equals("apply");
        boolean adjacentRet = args[3].equals("adjacent-ret");
        int maximum = Integer.parseInt(args[2]);
        if (maximum < 0 || (!apply && maximum != 0)) {
            throw new IllegalArgumentException("invalid promotion limit");
        }

        // Snapshot all references before decoding: a recovered body might
        // expose further CALLs, which must be reviewed in another run.
        Map<Long, List<Long>> sitesByTarget = new TreeMap<>();
        InstructionIterator instructions = currentProgram.getListing().getInstructions(true);
        while (instructions.hasNext()) {
            monitor.checkCancelled();
            Instruction call = instructions.next();
            Address site = call.getAddress();
            if (!inCode(site) || !call.getMnemonicString().equals("CALL") ||
                    call.getLength() != 5 || readByte(site.getOffset()) != 0xe8 ||
                    getFunctionContaining(site) == null) continue;
            long target = site.getOffset() + 5 + currentProgram.getMemory().getInt(toAddr(site.getOffset() + 1));
            if (target < 0x100 || target >= CODE_END ||
                    getFunctionContaining(toAddr(target)) != null) continue;
            // Require an exact Ghidra CALL reference agreeing with the raw
            // little-endian E8 displacement, not just an apparent byte pattern.
            for (Reference reference : call.getReferencesFrom()) {
                if (reference.getReferenceType().isCall() &&
                        reference.getToAddress().equals(toAddr(target))) {
                    sitesByTarget.computeIfAbsent(target, key -> new ArrayList<>())
                                 .add(site.getOffset());
                    break;
                }
            }
        }

        int promoted = 0, failed = 0;
        List<String> rows = new ArrayList<>();
        rows.add("entry\tcall_sites\treference_count\tfirst_opcode\tpadding\tconfidence\tresult");
        for (Map.Entry<Long, List<Long>> targetSites : sitesByTarget.entrySet()) {
            monitor.checkCancelled();
            long entry = targetSites.getKey();
            String padding = padding(entry);
            int opcode = readByte(entry);
            boolean guarded = isSafeCandidate(entry, padding, opcode, adjacentRet);
            String confidence = guarded ?
                (adjacentRet ? "call_ret_and_prologue" : "call_padding_and_prologue") :
                "insufficient_evidence";
            String result = guarded ? "proposed" : "not_selected";
            if (guarded && apply) {
                if (maximum > 0 && promoted >= maximum) {
                    result = "promotion_limited";
                } else if (getFunctionContaining(toAddr(entry)) != null) {
                    result = "covered_by_new_function";
                } else {
                    // Ghidra's disassembler can leave partial instructions on
                    // failure. Keep both decoding and function creation inside
                    // a rollback-capable per-target transaction.
                    int transaction = currentProgram.startTransaction("Recover EF2000 CALL target");
                    boolean commit = false;
                    try {
                        if (disassembleBounded(entry)) {
                            Function function = createFunction(toAddr(entry),
                                String.format("candidate_direct_call_%08x", entry));
                            if (function != null) {
                                result = "created";
                                ++promoted;
                                commit = true;
                            } else {
                                result = "create_failed";
                                ++failed;
                            }
                        } else {
                            result = "disassemble_failed";
                            ++failed;
                        }
                    } catch (Exception error) {
                        result = "create_failed";
                        ++failed;
                    } finally {
                        currentProgram.endTransaction(transaction, commit);
                    }
                }
            }
            List<String> callSites = new ArrayList<>();
            for (long site : targetSites.getValue()) {
                callSites.add(String.format("0x%08x", site));
            }
            rows.add(String.format("0x%08x\t%s\t%d\t0x%02x\t%s\t%s\t%s",
                entry, String.join(",", callSites), callSites.size(),
                opcode, padding, confidence, result));
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", rows) + "\n", StandardCharsets.UTF_8);
        println("EF2000: " + sitesByTarget.size() + " unresolved direct-CALL targets, " +
                promoted + " created, " + failed + " failed; wrote " + output);
    }

    private boolean inCode(Address address) {
        return address.getAddressSpace().equals(currentProgram.getAddressFactory().getDefaultAddressSpace()) &&
               address.getOffset() >= 0 && address.getOffset() < CODE_END;
    }

    private int readByte(long offset) throws Exception {
        return currentProgram.getMemory().getByte(toAddr(offset)) & 0xff;
    }

    private String padding(long entry) throws Exception {
        // Watcom emits MOV EAX,EAX or LEA EAX,[EAX] to align function starts.
        if (entry >= 2 && readByte(entry - 2) == 0x8b && readByte(entry - 1) == 0xc0) {
            return "mov_eax_eax";
        }
        if (entry >= 3 && readByte(entry - 3) == 0x8d && readByte(entry - 2) == 0x40 &&
                readByte(entry - 1) == 0x00) {
            return "lea_eax_eax";
        }
        return "none";
    }

    private boolean isSafeCandidate(long entry, String padding, int opcode,
                                    boolean adjacentRet) throws Exception {
        boolean boundary = adjacentRet ? readByte(entry - 1) == 0xc3 : !padding.equals("none");
        if ((entry & 3) != 0 || !boundary ||
                getFunctionContaining(toAddr(entry)) != null ||
                getInstructionContaining(toAddr(entry)) != null ||
                currentProgram.getListing().getDefinedDataContaining(toAddr(entry)) != null) return false;
        // A saved-register prologue, reached by an exact direct CALL, is a
        // deliberately narrow seed. Avoid MOV/TEST starts and jump interiors.
        return opcode == 0x53 || opcode == 0x55 || opcode == 0x56 || opcode == 0x57 ||
               opcode == 0x52 || opcode == 0x51;
    }

    private boolean disassembleBounded(long entry) throws Exception {
        Instruction next = getInstructionAfter(toAddr(entry));
        long end = Math.min(CODE_END, entry + MAX_DISASSEMBLY_BYTES);
        if (next != null) end = Math.min(end, next.getAddress().getOffset());
        // Never allow a decode through preexisting typed data, even if it
        // appears between two separately known instruction regions.
        for (long offset = entry; offset < end; ++offset) {
            if (currentProgram.getListing().getDefinedDataContaining(toAddr(offset)) != null) {
                end = offset;
                break;
            }
        }
        if (end <= entry) return false;
        DisassembleCommand command = new DisassembleCommand(toAddr(entry),
            new AddressSet(toAddr(entry), toAddr(end - 1)), true);
        return command.applyTo(currentProgram, monitor) && getInstructionAt(toAddr(entry)) != null;
    }
}
