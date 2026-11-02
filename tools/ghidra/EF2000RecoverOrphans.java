// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
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

/** Conservatively find jump/call-referenced orphan routines. Dry-run by default. */
public class EF2000RecoverOrphans extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3) {
            throw new IllegalArgumentException("expected report path, report|apply, max promotions (0=all)");
        }
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected pinned zero-based EF2000 X-32 image");
        }
        boolean apply = args[1].equals("apply");
        if (!apply && !args[1].equals("report")) {
            throw new IllegalArgumentException("expected report or apply");
        }
        int maximum = Integer.parseInt(args[2]);
        if (maximum < 0) throw new IllegalArgumentException("negative max promotions");
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());

        // Snapshot candidates *before* creating any new functions: otherwise
        // one newly recovered body can change which later addresses appear
        // unassigned, making a pass dependent on iteration side effects.
        List<Candidate> candidates = new ArrayList<>();
        Instruction previous = null;
        long orphanEnd = -1;
        InstructionIterator iterator = currentProgram.getListing().getInstructions(true);
        while (iterator.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = iterator.next();
            Address address = instruction.getAddress();
            if (!inImage(address)) continue;
            boolean unassigned = getFunctionContaining(address) == null;
            boolean rangeStart = unassigned && address.getOffset() != orphanEnd;
            boolean afterReturn = unassigned && previous != null &&
                previous.getMnemonicString().startsWith("RET") &&
                previous.getAddress().getOffset() + previous.getLength() == address.getOffset();
            if (rangeStart || afterReturn) {
                int jumps = 0, calls = 0, data = 0;
                StringBuilder examples = new StringBuilder();
                for (Reference reference : getReferencesTo(address)) {
                    if (reference.getReferenceType().isJump()) ++jumps;
                    else if (reference.getReferenceType().isCall()) ++calls;
                    else ++data;
                    if (examples.length() < 200) {
                        if (examples.length() > 0) examples.append(",");
                        examples.append(String.format("0x%08x:%s", reference.getFromAddress().getOffset(),
                                                      reference.getReferenceType()));
                    }
                }
                boolean prologue = isPrologue(instruction);
                boolean strong = prologue && (jumps > 0 || calls > 0 || data > 0);
                candidates.add(new Candidate(address, rangeStart ? "range_start" : "after_return",
                                             instruction.toString(), jumps, calls, data,
                                             examples.toString(), strong));
            }
            orphanEnd = unassigned ? address.getOffset() + instruction.getLength() : -1;
            previous = instruction;
        }

        int strongCount = 0, promoted = 0, skipped = 0, failed = 0;
        List<String> rows = new ArrayList<>();
        rows.add("entry\treason\tfirst_instruction\tjump_refs\tcall_refs\tdata_refs\tconfidence\tresult\txref_examples");
        for (Candidate candidate : candidates) {
            monitor.checkCancelled();
            if (candidate.strong) ++strongCount;
            String result = "not_selected";
            if (candidate.strong) {
                result = "proposed";
                if (apply) {
                    if (maximum > 0 && promoted >= maximum) {
                        result = "limited";
                    } else if (getFunctionContaining(candidate.address) != null) {
                        result = "covered_by_new_function";
                        ++skipped;
                    } else {
                        try {
                            // This labels a *candidate*, not a claimed name or
                            // type. Only an explicit --apply mutates Ghidra.
                            Function function = createFunction(candidate.address,
                                String.format("candidate_orphan_%08x", candidate.address.getOffset()));
                            if (function == null) {
                                result = "create_failed";
                                ++failed;
                            } else {
                                result = "created";
                                ++promoted;
                            }
                        } catch (Exception error) {
                            result = "create_failed";
                            ++failed;
                        }
                    }
                }
            }
            rows.add(String.format("0x%08x\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s",
                candidate.address.getOffset(), candidate.reason, clean(candidate.firstInstruction),
                candidate.jumps, candidate.calls, candidate.data,
                candidate.strong ? "strong_reference_and_prologue" : "weak", result,
                candidate.xrefs));
        }
        Files.writeString(output, String.join("\n", rows) + "\n", StandardCharsets.UTF_8);
        println("EF2000: " + candidates.size() + " orphan candidates, " + strongCount +
                " reference/prologue matches, " + promoted + " created, " + skipped +
                " already covered, " + failed + " failed; wrote " + output);
    }

    private boolean inImage(Address address) {
        return address.getAddressSpace().equals(currentProgram.getAddressFactory().getDefaultAddressSpace()) &&
               address.getOffset() >= 0 && address.getOffset() < IMAGE_END;
    }

    private boolean isPrologue(Instruction instruction) {
        String text = instruction.toString();
        return text.startsWith("PUSH EBX") || text.startsWith("PUSH EBP") ||
               text.startsWith("PUSH ESI") || text.startsWith("PUSH EDI") ||
               text.startsWith("SUB ESP,") || text.startsWith("ENTER ");
    }

    private String clean(String text) {
        return text.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private static class Candidate {
        final Address address;
        final String reason, firstInstruction, xrefs;
        final int jumps, calls, data;
        final boolean strong;

        Candidate(Address address, String reason, String firstInstruction,
                  int jumps, int calls, int data, String xrefs, boolean strong) {
            this.address = address;
            this.reason = reason;
            this.firstInstruction = firstInstruction;
            this.jumps = jumps;
            this.calls = calls;
            this.data = data;
            this.xrefs = xrefs;
            this.strong = strong;
        }
    }
}
