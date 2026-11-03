// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Map direct memcpy calls and candidate calls through the graphics copy slot. */
public class EF2000CopyCallers extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            throw new IllegalArgumentException("expected TSV output path");
        }
        List<String> rows = new ArrayList<>();
        for (Reference reference : getReferencesTo(toAddr(0xe9a24))) {
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            addCall(rows, reference.getFromAddress(), "direct");
        }
        // The graphics code performs additional large copies through a
        // mutable function pointer. These are candidates, not guaranteed
        // memcpy calls: verify their runtime destination with --profile-copy-callers.
        for (Reference reference : getReferencesTo(toAddr(0x41f3d0))) {
            addCall(rows, reference.getFromAddress(), "indirect_candidate");
        }
        Collections.sort(rows);
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        String header = "call_site\treturn_ip\tfunction_entry\tfunction_name\tkind\n";
        Files.writeString(output, header + String.join("\n", rows) + "\n",
                          StandardCharsets.UTF_8);
        println("EF2000: wrote " + rows.size() + " copy call candidates to " + output);
    }

    private void addCall(List<String> rows, Address callSite, String kind) {
        Instruction instruction = getInstructionAt(callSite);
        if (instruction == null || !instruction.getFlowType().isCall()) {
            return;
        }
        Address returnIp = callSite.add(instruction.getLength());
        Function caller = getFunctionContaining(callSite);
        String entry = caller == null ? "" :
            String.format("0x%08x", caller.getEntryPoint().getOffset());
        String name = caller == null ? "" : caller.getName();
        rows.add(String.format("0x%08x\t0x%08x\t%s\t%s\t%s",
                               callSite.getOffset(), returnIp.getOffset(), entry, name,
                               kind));
    }
}
