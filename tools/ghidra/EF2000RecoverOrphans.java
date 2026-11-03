// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
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
        if (args.length != 4) {
            throw new IllegalArgumentException(
                "expected report path, report|apply, max promotions (0=all), strict|table");
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
        String tier = args[3];
        if (!tier.equals("strict") && !tier.equals("table")) {
            throw new IllegalArgumentException("expected strict or table tier");
        }
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
                boolean alignedTableRef = false;
                StringBuilder examples = new StringBuilder();
                for (Reference reference : getReferencesTo(address)) {
                    if (reference.getReferenceType().isJump()) ++jumps;
                    else if (reference.getReferenceType().isCall()) ++calls;
                    else {
                        ++data;
                        long from = reference.getFromAddress().getOffset();
                        if (reference.getReferenceType().isData() && inImage(reference.getFromAddress()) &&
                                from >= 0x110000 && (from & 3) == 0) {
                            alignedTableRef = true;
                        }
                    }
                    if (examples.length() < 200) {
                        if (examples.length() > 0) examples.append(",");
                        examples.append(String.format("0x%08x:%s", reference.getFromAddress().getOffset(),
                                                      reference.getReferenceType()));
                    }
                }
                boolean prologue = isPrologue(instruction);
                String confidence = prologue && (jumps > 0 || calls > 0 || data > 0) ?
                    "strong_reference_and_prologue" : "weak";
                if (confidence.equals("weak") && tier.equals("table") && alignedTableRef &&
                        (address.getOffset() & 3) == 0 && isVolatilePush(instruction)) {
                    // Watcom can use EDX/ECX/EAX as register parameters, so
                    // these may be callback entry points even though the
                    // first PUSH is not a conventional saved-register prologue.
                    // Require an aligned source inside the initialized data
                    // area to avoid arbitrary branch labels and bad disassembly.
                    confidence = "aligned_table_reference_and_push";
                }
                candidates.add(new Candidate(address, rangeStart ? "range_start" : "after_return",
                                             instruction.toString(), jumps, calls, data,
                                             examples.toString(), confidence));
            }
            orphanEnd = unassigned ? address.getOffset() + instruction.getLength() : -1;
            previous = instruction;
        }

        int strongCount = 0, promoted = 0, skipped = 0, failed = 0;
        List<String> rows = new ArrayList<>();
        rows.add("entry\treason\tfirst_instruction\tjump_refs\tcall_refs\tdata_refs\tconfidence\tresult\txref_examples");
        for (Candidate candidate : candidates) {
            monitor.checkCancelled();
            if (!candidate.confidence.equals("weak")) ++strongCount;
            String result = "not_selected";
            if (!candidate.confidence.equals("weak")) {
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
                candidate.confidence, result,
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

    private boolean isVolatilePush(Instruction instruction) {
        String text = instruction.toString();
        return text.equals("PUSH EAX") || text.equals("PUSH ECX") || text.equals("PUSH EDX");
    }

    private String clean(String text) {
        return text.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private static class Candidate {
        final Address address;
        final String reason, firstInstruction, xrefs, confidence;
        final int jumps, calls, data;

        Candidate(Address address, String reason, String firstInstruction,
                  int jumps, int calls, int data, String xrefs, String confidence) {
            this.address = address;
            this.reason = reason;
            this.firstInstruction = firstInstruction;
            this.jumps = jumps;
            this.calls = calls;
            this.data = data;
            this.xrefs = xrefs;
            this.confidence = confidence;
        }
    }
}
