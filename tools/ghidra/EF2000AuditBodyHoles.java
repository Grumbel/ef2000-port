// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/** Read-only inventory of the holes between already-owned function-body ranges. */
public class EF2000AuditBodyHoles extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("expected output.tsv [guest-offset ...]");
        List<String> rows = new ArrayList<>();
        rows.add("entry\tfrom\tthrough\tbytes\tinstructions\tundefined_bytes\tdata_bytes\tother_function_bytes\treferences\tinstruction_window\tdata_window");
        if (args.length == 1) {
            for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
                monitor.checkCancelled();
                inspect(function, rows);
            }
        } else {
            for (int index = 1; index < args.length; index++) {
                Function function = getFunctionAt(toAddr(Long.parseUnsignedLong(args[index], 16)));
                if (function == null) throw new IllegalArgumentException("no function at " + args[index]);
                inspect(function, rows);
            }
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", rows) + "\n", StandardCharsets.UTF_8);
        println("EF2000: wrote " + (rows.size() - 1) + " body-hole rows to " + output);
    }

    private void inspect(Function function, List<String> rows) throws Exception {
        if (function.getBody().getNumAddressRanges() < 2) return;
        AddressRangeIterator iter = function.getBody().getAddressRanges();
        AddressRange previous = iter.next();
        while (iter.hasNext()) {
            AddressRange next = iter.next();
            long from = previous.getMaxAddress().getOffset() + 1;
            long through = next.getMinAddress().getOffset() - 1;
            if (through >= from) inspectGap(function, from, through, rows);
            previous = next;
        }
    }

    private void inspectGap(Function function, long from, long through, List<String> rows) throws Exception {
        int instructions = 0, undefined = 0, data = 0, other = 0;
        List<String> references = new ArrayList<>(), window = new ArrayList<>(), dataWindow = new ArrayList<>();
        for (long offset = from; offset <= through; offset++) {
            monitor.checkCancelled();
            Instruction instruction = getInstructionContaining(toAddr(offset));
            if (instruction == null) {
                Data item = currentProgram.getListing().getDefinedDataContaining(toAddr(offset));
                if (item != null) {
                    data++;
                    if (item.getAddress().getOffset() == offset && dataWindow.size() < 16) {
                        dataWindow.add(String.format("0x%x:%s(+%d)", offset,
                            item.getDataType().getName(), item.getLength()));
                    }
                } else undefined++;
            } else if (instruction.getAddress().getOffset() == offset) {
                instructions++;
                if (window.size() < 32) window.add(String.format("0x%x:%s(+%d)", offset,
                    clean(instruction.toString()), instruction.getLength()));
            }
            Function owner = getFunctionContaining(toAddr(offset));
            if (owner != null && owner != function) other++;
            for (Reference ref : getReferencesTo(toAddr(offset))) {
                if (references.size() < 32) references.add(String.format("0x%x<-0x%x:%s:%s",
                    offset, ref.getFromAddress().getOffset(), ref.getReferenceType(), ref.getSource()));
            }
        }
        rows.add(String.format("0x%08x\t0x%08x\t0x%08x\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s",
            function.getEntryPoint().getOffset(), from, through, through - from + 1,
            instructions, undefined, data, other, String.join(" | ", references),
            String.join(" | ", window), String.join(" | ", dataWindow)));
    }

    private String clean(String text) {
        return text.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }
}
