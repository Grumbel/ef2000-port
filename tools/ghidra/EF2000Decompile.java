// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

/** Export focused Ghidra pseudo-C and incoming references by guest offset. */
public class EF2000Decompile extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("expected output directory and guest offsets");
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) {
            throw new IllegalStateException("cannot initialize Ghidra decompiler");
        }
        try {
            for (int i = 1; i < args.length; ++i) {
                long offset = Long.parseUnsignedLong(args[i], 16);
                Address address = toAddr(offset);
                Function function = getFunctionAt(address);
                // A sampled basic-block address usually lands *inside* a
                // function. Prefer the discovered containing function to an
                // incorrect new function beginning at the sampled block.
                if (function == null) {
                    function = getFunctionContaining(address);
                }
                if (function == null) {
                    if (getInstructionAt(address) == null && !disassemble(address)) {
                        throw new IllegalStateException("cannot disassemble " + address);
                    }
                    function = createFunction(address, String.format("manual_%08x", offset));
                }
                if (function == null) {
                    throw new IllegalStateException("no function at " + address);
                }
                DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
                if (!result.decompileCompleted() || result.getDecompiledFunction() == null) {
                    throw new IllegalStateException("decompilation failed at " + address + ": " +
                                                    result.getErrorMessage());
                }
                StringBuilder code = new StringBuilder();
                code.append("/* EF2000 X-32 pseudo-C; NOT verified source code.\n");
                code.append(String.format(" * Requested guest offset: 0x%08x\n", offset));
                code.append(" * Function entry: ").append(function.getEntryPoint())
                    .append("; function: ").append(function.getName()).append("\n");
                code.append(" * Compiler model is generic x86, NOT the Watcom X-32 ABI.\n");
                code.append(" * Incoming references (may include data/false positives):\n");
                for (Reference reference : getReferencesTo(function.getEntryPoint())) {
                    code.append(" *   ").append(reference.getFromAddress()).append(" ")
                        .append(reference.getReferenceType()).append("\n");
                }
                code.append(" */\n\n");
                code.append(result.getDecompiledFunction().getC());
                Path file = output.resolve(String.format("0x%08x.c", offset));
                Files.writeString(file, code.toString(), StandardCharsets.UTF_8);
                println("EF2000: wrote " + file);
            }
        } finally {
            decompiler.dispose();
        }
    }
}
