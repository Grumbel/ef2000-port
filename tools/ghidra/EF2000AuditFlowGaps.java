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

/** Read-only inventory of suspicious fall-through gaps after recognized code. */
public class EF2000AuditFlowGaps extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;
    private static final long CODE_END = 0x110000L;
    private static final int MAX_GAP = 16;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("expected report.tsv");
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(0)) == null ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected pinned zero-based EF2000 X-32 image");
        }

        List<String> report = new ArrayList<>();
        report.add("function_entry\tsite\tfallthrough\tnext_orphan\tgap_bytes\tfirst_byte\tprevious_instruction\tnext_instruction\torphan_window\torphan_references");
        InstructionIterator instructions = currentProgram.getListing().getInstructions(true);
        while (instructions.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = instructions.next();
            Address site = instruction.getAddress();
            if (!inCode(site)) continue;
            Function function = getFunctionContaining(site);
            if (function == null) continue;
            Address fallthrough = instruction.getFallThrough();
            if (fallthrough == null || !inCode(fallthrough) ||
                    getFunctionContaining(fallthrough) != null ||
                    getInstructionAt(fallthrough) != null ||
                    currentProgram.getListing().getDefinedDataContaining(fallthrough) != null) continue;
            Instruction next = getInstructionAfter(fallthrough);
            if (next == null || !inCode(next.getAddress()) ||
                    getFunctionContaining(next.getAddress()) != null) continue;
            long gap = next.getAddress().getOffset() - fallthrough.getOffset();
            if (gap < 1 || gap > MAX_GAP) continue;
            int opcode = currentProgram.getMemory().getByte(fallthrough) & 0xff;
            report.add(String.format("0x%08x\t0x%08x\t0x%08x\t0x%08x\t%d\t0x%02x\t%s\t%s\t%s\t%s",
                function.getEntryPoint().getOffset(), site.getOffset(),
                fallthrough.getOffset(), next.getAddress().getOffset(),
                gap, opcode, clean(instruction.toString()), clean(next.toString()),
                orphanWindow(next), orphanReferences(next)));
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", report) + "\n", StandardCharsets.UTF_8);
        println("EF2000: " + (report.size() - 1) + " short orphan fall-through gaps; wrote " + output);
    }

    private boolean inCode(Address address) {
        return address.getAddressSpace().equals(currentProgram.getAddressFactory().getDefaultAddressSpace()) &&
               address.getOffset() >= 0 && address.getOffset() < CODE_END;
    }

    private String clean(String text) {
        return text.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }

    private String orphanWindow(Instruction start) {
        List<String> items = new ArrayList<>();
        Instruction instruction = start;
        for (int i = 0; i < 16 && instruction != null; i++) {
            if (getFunctionContaining(instruction.getAddress()) != null ||
                    instruction.getAddress().getOffset() > start.getAddress().getOffset() + 96) break;
            items.add(String.format("0x%x(+%d):%s", instruction.getAddress().getOffset(),
                                    instruction.getLength(), clean(instruction.toString())));
            instruction = getInstructionAfter(instruction.getAddress());
        }
        return String.join(" | ", items);
    }

    private String orphanReferences(Instruction start) {
        List<String> refs = new ArrayList<>();
        for (long offset = start.getAddress().getOffset();
                offset < start.getAddress().getOffset() + 48; offset++) {
            for (Reference ref : getReferencesTo(toAddr(offset))) {
                refs.add(String.format("0x%x<-0x%x:%s:%s", offset,
                    ref.getFromAddress().getOffset(), ref.getReferenceType(), ref.getSource()));
            }
        }
        return String.join(" | ", refs);
    }
}
