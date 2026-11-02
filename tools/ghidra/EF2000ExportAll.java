// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.Variable;
import ghidra.program.model.listing.VariableStorage;
import ghidra.program.model.data.DWordDataType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.lang.PrototypeModel;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/** Inventory all discovered functions/calls and optionally export every decompilation. */
public class EF2000ExportAll extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 6) {
            throw new IllegalArgumentException(
                "expected output directory, inventory|decompile, maximum functions (0=all), resume|overwrite, none|fastcall|watcom, none|storage");
        }
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(0)) == null ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected the zero-based EF2000 X-32 image");
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        boolean export = args[1].equals("decompile");
        if (!export && !args[1].equals("inventory")) {
            throw new IllegalArgumentException("expected inventory or decompile");
        }
        int maximum = Integer.parseInt(args[2]);
        if (maximum < 0) throw new IllegalArgumentException("negative function limit");
        boolean overwrite = args[3].equals("overwrite");
        if (!overwrite && !args[3].equals("resume")) {
            throw new IllegalArgumentException("expected resume or overwrite");
        }
        String fallbackMode = args[4];
        if (!fallbackMode.equals("none") && !fallbackMode.equals("fastcall") &&
                !fallbackMode.equals("watcom")) {
            throw new IllegalArgumentException("expected none, fastcall, or watcom fallback");
        }
        if (fallbackMode.equals("watcom")) {
            boolean available = false;
            for (PrototypeModel model : currentProgram.getCompilerSpec().getCallingConventions()) {
                if (model.getName().equals("__ef2000_aedb")) available = true;
            }
            if (!available) throw new IllegalStateException(
                "missing experimental Watcom model; prepare a private Ghidra copy with ghidra_watcom.py");
        }
        if (!args[5].equals("none") && !args[5].equals("storage")) {
            throw new IllegalArgumentException("expected none or storage fallback");
        }
        boolean storageFallback = args[5].equals("storage");

        List<Function> functions = new ArrayList<>();
        FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
        while (iterator.hasNext()) {
            Function function = iterator.next();
            if (!function.isExternal() && inImage(function.getEntryPoint())) functions.add(function);
        }

        // These numbers describe Ghidra's *current static analysis*, not
        // a proof that every game routine has been found. The image mixes
        // executable instructions, lookup tables and embedded resources.
        long instructionCount = 0, instructionBytes = 0, orphanInstructionBytes = 0;
        long callSites = 0, directCalls = 0, unresolvedDirectCalls = 0, indirectCalls = 0;
        List<String> calls = new ArrayList<>();
        calls.add("caller_entry\tcall_site\ttarget\tcallee_entry\tkind");
        List<String> orphanRanges = new ArrayList<>();
        orphanRanges.add("start\tend_exclusive\tinstruction_count\tbytes");
        List<String> candidateEntries = new ArrayList<>();
        candidateEntries.add("entry\treason\tfirst_instruction");
        StringBuilder orphanAssembly = new StringBuilder(
            "; Disassembled bytes without a recognized function; NOT confirmed game routines.\n");
        long orphanStart = -1, orphanEnd = -1, orphanCount = 0, orphanBytes = 0;
        InstructionIterator instructions = currentProgram.getListing().getInstructions(true);
        Instruction predecessor = null;
        while (instructions.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = instructions.next();
            Instruction previous = predecessor;
            predecessor = instruction;
            Address site = instruction.getAddress();
            if (!inImage(site)) continue;
            ++instructionCount;
            instructionBytes += instruction.getLength();
            Function caller = getFunctionContaining(site);
            if (caller == null) {
                orphanInstructionBytes += instruction.getLength();
                boolean rangeStart = orphanStart < 0 || site.getOffset() != orphanEnd;
                boolean afterReturn = previous != null &&
                    previous.getMnemonicString().startsWith("RET") &&
                    previous.getAddress().getOffset() + previous.getLength() == site.getOffset();
                if (rangeStart || afterReturn) {
                    candidateEntries.add(address(site) + "\t" +
                                         (rangeStart ? "unassigned_range_start" : "after_return") +
                                         "\t" + clean(instruction.toString()));
                }
                if (orphanStart >= 0 && site.getOffset() != orphanEnd) {
                    orphanRanges.add(orphanRange(orphanStart, orphanEnd, orphanCount, orphanBytes));
                    orphanStart = -1;
                }
                if (orphanStart < 0) {
                    orphanStart = site.getOffset();
                    orphanCount = orphanBytes = 0;
                }
                orphanEnd = site.getOffset() + instruction.getLength();
                ++orphanCount;
                orphanBytes += instruction.getLength();
                orphanAssembly.append(address(site)).append("  ").append(instruction).append('\n');
            } else if (orphanStart >= 0) {
                orphanRanges.add(orphanRange(orphanStart, orphanEnd, orphanCount, orphanBytes));
                orphanStart = -1;
            }
            if (!instruction.getFlowType().isCall()) continue;
            ++callSites;
            String callerEntry = caller == null ? "" : address(caller.getEntryPoint());
            boolean resolved = false;
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall() || !inImage(reference.getToAddress())) {
                    continue;
                }
                resolved = true;
                ++directCalls;
                Function callee = getFunctionContaining(reference.getToAddress());
                if (callee == null) ++unresolvedDirectCalls;
                calls.add(callerEntry + "\t" + address(site) + "\t" +
                          address(reference.getToAddress()) + "\t" +
                          (callee == null ? "" : address(callee.getEntryPoint())) + "\t" +
                          (callee == null ? "unresolved_direct" : "direct"));
            }
            if (!resolved) {
                ++indirectCalls;
                calls.add(callerEntry + "\t" + address(site) + "\t\t\tindirect_or_external");
            }
        }
        if (orphanStart >= 0) {
            orphanRanges.add(orphanRange(orphanStart, orphanEnd, orphanCount, orphanBytes));
        }

        DecompInterface decompiler = null;
        DecompInterface alternativeDecompiler = null;
        if (export) {
            decompiler = new DecompInterface();
            if (!decompiler.openProgram(currentProgram)) {
                throw new IllegalStateException("cannot initialize Ghidra decompiler");
            }
            if (!fallbackMode.equals("none")) {
                alternativeDecompiler = new DecompInterface();
                DecompileOptions options = new DecompileOptions();
                options.setProtoEvalModel(fallbackMode.equals("watcom") ? "__ef2000_aedb" :
                                          "__fastcall");
                alternativeDecompiler.setOptions(options);
                if (!alternativeDecompiler.openProgram(currentProgram)) {
                    throw new IllegalStateException("cannot initialize alternative decompiler");
                }
            }
        }
        long functionBytes = 0;
        int written = 0, reused = 0, failed = 0, limited = 0, assemblyFallbacks = 0;
        int fastcallWritten = 0, watcomWritten = 0, storageWritten = 0;
        List<String> index = new ArrayList<>();
        index.add("entry\tname\tbody_bytes\tstatus\tfile\terror");
        try {
            for (Function function : functions) {
                monitor.checkCancelled();
                long offset = function.getEntryPoint().getOffset();
                functionBytes += function.getBody().getNumAddresses();
                String fileName = String.format("functions/%04x/0x%08x.c", offset >>> 16, offset);
                Path file = output.resolve(fileName);
                String status = "inventory", error = "", relative = "";
                if (export) {
                    relative = fileName;
                    if (maximum > 0 && written + reused + failed >= maximum) {
                        status = "limited";
                        ++limited;
                    } else if (!overwrite && Files.isRegularFile(file)) {
                        status = "reused";
                        ++reused;
                    } else {
                        try {
                            DecompileResults result = decompiler.decompileFunction(function, 30, monitor);
                            boolean usedAlternative = false;
                            if ((!result.decompileCompleted() || result.getDecompiledFunction() == null) &&
                                    alternativeDecompiler != null) {
                                DecompileResults alternative =
                                    alternativeDecompiler.decompileFunction(function, 30, monitor);
                                if (alternative.decompileCompleted() &&
                                        alternative.getDecompiledFunction() != null) {
                                    result = alternative;
                                    usedAlternative = true;
                                }
                            }
                            String pseudoC = result.decompileCompleted() &&
                                result.getDecompiledFunction() != null ?
                                result.getDecompiledFunction().getC() : null;
                            boolean usedStorage = false;
                            if (pseudoC == null && storageFallback) {
                                pseudoC = decompileWithTemporaryStorage(function);
                                usedStorage = pseudoC != null;
                            }
                            if (pseudoC == null) {
                                status = "failed";
                                error = result.getErrorMessage();
                                ++failed;
                            } else {
                                String header = String.format(
                                    "/* Ghidra pseudo-C, not original EF2000 source.\n" +
                                    " * Guest entry 0x%08x; %s; generic x86 ABI, not Watcom X-32.\n" +
                                    (usedAlternative ?
                                     " * WARNING: default decompilation failed; C uses experimental " +
                                     (fallbackMode.equals("watcom") ?
                                      "Watcom-like EAX/EDX/EBX/ECX" : "__fastcall") +
                                     " parameter evaluation.\n" +
                                     " * This is NOT a proven calling convention or validated original source.\n" :
                                     "") +
                                    (usedStorage ?
                                     " * WARNING: default/alternate decompilation failed; C uses a temporary\n" +
                                     " * custom signature pinning EAX, EDX, EBX, ECX as 32-bit inputs.\n" +
                                     " * Stack arguments, call sites, and the ABI are NOT verified.\n" : "") +
                                    " */\n\n", offset, function.getName());
                                Files.createDirectories(file.getParent());
                                Files.writeString(file, header + pseudoC,
                                                  StandardCharsets.UTF_8);
                                status = usedStorage ? "written_storage_fallback" :
                                         usedAlternative ? "written_" + fallbackMode + "_fallback" :
                                         "written";
                                if (usedStorage) ++storageWritten;
                                else if (usedAlternative) {
                                    if (fallbackMode.equals("watcom")) ++watcomWritten;
                                    else ++fastcallWritten;
                                }
                                ++written;
                            }
                        } catch (Exception exception) {
                            status = "failed";
                            error = exception.getMessage();
                            ++failed;
                        }
                        if (status.equals("failed")) {
                            // Retain executable-level evidence even when the
                            // high-level decompiler cannot reconcile Watcom
                            // register overlaps or malformed flow graphs.
                            // A forced refresh must not leave stale pseudo-C
                            // that the next resume pass would silently reuse.
                            if (overwrite) Files.deleteIfExists(file);
                            relative = fileName.replace(".c", ".asm");
                            Path assemblyFile = output.resolve(relative);
                            Files.createDirectories(assemblyFile.getParent());
                            Files.writeString(assemblyFile, disassembly(function),
                                              StandardCharsets.UTF_8);
                            ++assemblyFallbacks;
                        }
                    }
                    if ((written + reused + failed) % 100 == 0 &&
                            (written + reused + failed) > 0 && !status.equals("limited")) {
                        println("EF2000: decompiled/checked " + (written + reused + failed) +
                                "/" + functions.size() + " functions");
                    }
                }
                index.add(address(function.getEntryPoint()) + "\t" + function.getName() + "\t" +
                          function.getBody().getNumAddresses() + "\t" + status + "\t" +
                          relative + "\t" + clean(error));
            }
        } finally {
            if (decompiler != null) decompiler.dispose();
            if (alternativeDecompiler != null) alternativeDecompiler.dispose();
        }
        Files.writeString(output.resolve("functions.tsv"), String.join("\n", index) + "\n",
                          StandardCharsets.UTF_8);
        Files.writeString(output.resolve("calls.tsv"), String.join("\n", calls) + "\n",
                          StandardCharsets.UTF_8);
        Files.writeString(output.resolve("orphan-ranges.tsv"), String.join("\n", orphanRanges) + "\n",
                          StandardCharsets.UTF_8);
        Files.writeString(output.resolve("candidate-entries.tsv"),
                          String.join("\n", candidateEntries) + "\n", StandardCharsets.UTF_8);
        Files.writeString(output.resolve("orphan-instructions.asm"), orphanAssembly.toString(),
                          StandardCharsets.UTF_8);
        String report = String.format(
            "{\n  \"initialized_image_bytes\": %d,\n" +
            "  \"discovered_functions\": %d,\n  \"function_body_bytes_sum\": %d,\n" +
            "  \"disassembled_instructions\": %d,\n  \"instruction_bytes\": %d,\n" +
            "  \"instruction_bytes_outside_functions\": %d,\n" +
            "  \"call_sites\": %d,\n  \"direct_call_edges\": %d,\n" +
            "  \"unresolved_direct_call_edges\": %d,\n" +
            "  \"indirect_or_external_call_sites\": %d,\n" +
            "  \"decompilations_written\": %d,\n  \"fastcall_fallback_decompilations\": %d,\n" +
            "  \"watcom_fallback_decompilations\": %d,\n" +
            "  \"custom_storage_fallback_decompilations\": %d,\n" +
            "  \"decompilations_reused\": %d,\n" +
            "  \"decompilations_failed\": %d,\n  \"assembly_fallbacks\": %d,\n" +
            "  \"orphan_instruction_ranges\": %d,\n" +
            "  \"candidate_entries\": %d,\n  \"decompilations_limited\": %d\n}\n",
            IMAGE_END, functions.size(), functionBytes, instructionCount, instructionBytes,
            orphanInstructionBytes, callSites, directCalls, unresolvedDirectCalls,
            indirectCalls, written, fastcallWritten, watcomWritten, storageWritten,
            reused, failed, assemblyFallbacks,
            orphanRanges.size() - 1,
            candidateEntries.size() - 1, limited);
        Files.writeString(output.resolve("report.json"), report, StandardCharsets.UTF_8);
        println("EF2000: inventoried " + functions.size() + " functions, " +
                instructionBytes + " instruction bytes, " + unresolvedDirectCalls +
                " unresolved direct calls; wrote " + written + " (" + fastcallWritten +
                " fastcall, " + watcomWritten + " Watcom-like, " + storageWritten +
                " temporary-storage fallbacks), reused " + reused +
                ", failed " + failed + ", limited " + limited + " exports to " + output);
    }

    /** Retry only a failed function with explicit register inputs.
     *
     * Ghidra sometimes creates overlapping inferred input varnodes for X-32
     * packed stack accesses. A fully custom signature avoids that inference.
     * Explicitly restore the old signature before aborting the transaction,
     * including on exceptions. The caller must also use headless -readOnly:
     * a nested abort alone does not restore signatures in memory promptly.
     */
    private String decompileWithTemporaryStorage(Function function) throws Exception {
        String originalSignature = function.getPrototypeString(false, true);
        boolean originallyCustom = function.hasCustomVariableStorage();
        SourceType originalSource = function.getSignatureSource();
        List<Variable> originalParams = new ArrayList<>();
        for (Parameter p : function.getParameters()) {
            originalParams.add(new ParameterImpl(p.getName(), p.getDataType(),
                p.getVariableStorage(), currentProgram));
        }
        int transaction = currentProgram.startTransaction("temporary EF2000 decompiler inputs");
        try {
            List<Variable> params = new ArrayList<>();
            for (String name : new String[] {"EAX", "EDX", "EBX", "ECX"}) {
                Register register = currentProgram.getLanguage().getRegister(name);
                params.add(new ParameterImpl("param_" + name, DWordDataType.dataType,
                    new VariableStorage(currentProgram, register), currentProgram));
            }
            function.updateFunction(null, null, FunctionUpdateType.CUSTOM_STORAGE,
                                    true, SourceType.USER_DEFINED, params.toArray(new Variable[0]));
            DecompInterface temporary = new DecompInterface();
            try {
                if (!temporary.openProgram(currentProgram)) {
                    throw new IllegalStateException("cannot initialize temporary-storage decompiler");
                }
                DecompileResults result = temporary.decompileFunction(function, 30, monitor);
                return result.decompileCompleted() && result.getDecompiledFunction() != null ?
                    result.getDecompiledFunction().getC() : null;
            } finally {
                temporary.dispose();
            }
        } finally {
            try {
                // Headless scripts can have an outer transaction, in which
                // case a nested abort may not immediately restore its state.
                // Explicitly reinstate the original signature in memory too.
                function.updateFunction(null, null, originallyCustom ?
                    FunctionUpdateType.CUSTOM_STORAGE : FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS,
                    true, originalSource, originalParams.toArray(new Variable[0]));
            } finally {
                currentProgram.endTransaction(transaction, false);
            }
            Function restored = getFunctionAt(function.getEntryPoint());
            if (restored == null || restored.hasCustomVariableStorage() != originallyCustom ||
                    !restored.getPrototypeString(false, true).equals(originalSignature)) {
                throw new IllegalStateException("temporary signature not restored: before=" +
                    originalSignature + " custom=" + originallyCustom + " after=" +
                    (restored == null ? "missing" : restored.getPrototypeString(false, true)) +
                    " custom=" + (restored == null ? "missing" : restored.hasCustomVariableStorage()));
            }
        }
    }

    private boolean inImage(Address address) {
        return address.getAddressSpace().equals(currentProgram.getAddressFactory().getDefaultAddressSpace()) &&
               address.getOffset() >= 0 && address.getOffset() < IMAGE_END;
    }

    private String address(Address value) {
        return String.format("0x%08x", value.getOffset());
    }

    private String clean(String value) {
        return value == null ? "" : value.replace('\t', ' ').replace('\n', ' ').replace('\r', ' ');
    }

    private String orphanRange(long start, long end, long count, long bytes) {
        return String.format("0x%08x\t0x%08x\t%d\t%d", start, end, count, bytes);
    }

    private String disassembly(Function function) {
        StringBuilder result = new StringBuilder("; Ghidra could not decompile this function.\n");
        result.append("; Entry ").append(address(function.getEntryPoint()))
              .append("; ").append(function.getName()).append('\n');
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        while (instructions.hasNext()) {
            Instruction instruction = instructions.next();
            result.append(address(instruction.getAddress())).append("  ")
                  .append(instruction).append('\n');
        }
        return result.toString();
    }
}
