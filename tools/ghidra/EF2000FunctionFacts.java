// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;

import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashSet;
import java.util.Set;

/** Read-only, reproducible function/data reference facts for subsystem discovery. */
public class EF2000FunctionFacts extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;
    private static final long BSS_END = 0x4de2f0L;
    // The original flat image is followed by runtime BSS/heap globals;
    // constants in this range are only CANDIDATES, not inferred xrefs.
    private static final long GLOBAL_CANDIDATE_START = 0x110000L;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("expected output directory");
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(0)) == null ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected pinned zero-based EF2000 X-32 image");
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output);
        int count = 0;
        long dataReferences = 0, addressCandidates = 0, crossFunctionBranches = 0;
        try (BufferedWriter facts = Files.newBufferedWriter(output.resolve("function-facts.tsv"),
                     StandardCharsets.UTF_8);
             BufferedWriter refs = Files.newBufferedWriter(output.resolve("data-references.tsv"),
                     StandardCharsets.UTF_8);
             BufferedWriter candidates = Files.newBufferedWriter(
                     output.resolve("address-candidates.tsv"),
                     StandardCharsets.UTF_8);
             BufferedWriter branchesOut = Files.newBufferedWriter(
                     output.resolve("cross-function-branches.tsv"),
                     StandardCharsets.UTF_8)) {
            facts.write("entry\tname\tbody_bytes\tinstructions\tdirect_calls\tindirect_calls\t"
                + "conditional_branches\tpcode_loads\tpcode_stores\tunique_data_targets\t"
                + "data_reads\tdata_writes\tliteral_address_candidates\tcross_function_branches\n");
            refs.write("entry\tsite\ttarget\tref_type\tsource\ttarget_kind\t"
                + "data_type\tsymbol\n");
            candidates.write("entry\tsite\ttarget\toperand\tform\tregion\tsymbol\n");
            branchesOut.write("entry\tsite\ttarget\ttarget_entry\tkind\n");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                monitor.checkCancelled();
                Function function = functions.next();
                if (function.isExternal() || !inImage(function.getEntryPoint())) continue;
                long instructions = 0, calls = 0, indirect = 0, branches = 0;
                long loads = 0, stores = 0, reads = 0, writes = 0, literalCount = 0;
                long crossBranches = 0;
                Set<Long> dataTargets = new HashSet<>();
                InstructionIterator code = currentProgram.getListing().getInstructions(
                    function.getBody(), true);
                while (code.hasNext()) {
                    monitor.checkCancelled();
                    Instruction instruction = code.next();
                    ++instructions;
                    if (instruction.getFlowType().isCall()) {
                        boolean direct = false;
                        for (Reference ref : instruction.getReferencesFrom()) {
                            if (ref.getReferenceType().isCall() && inImage(ref.getToAddress())) {
                                direct = true;
                            }
                        }
                        if (direct) ++calls; else ++indirect;
                    }
                    if (instruction.getFlowType().isConditional()) ++branches;
                    for (PcodeOp op : instruction.getPcode()) {
                        if (op.getOpcode() == PcodeOp.LOAD) ++loads;
                        if (op.getOpcode() == PcodeOp.STORE) ++stores;
                    }
                    Set<Long> referencedHere = new HashSet<>();
                    for (Reference ref : instruction.getReferencesFrom()) {
                        Address target = ref.getToAddress();
                        if (ref.getReferenceType().isJump() && inImage(target)) {
                            Function owner = getFunctionContaining(target);
                            if (owner != null && owner != function && !owner.isExternal()) {
                                branchesOut.write(String.format("0x%08x\t0x%08x\t0x%08x\t0x%08x\tcross_function_jump\n",
                                    function.getEntryPoint().getOffset(),
                                    instruction.getAddress().getOffset(), target.getOffset(),
                                    owner.getEntryPoint().getOffset()));
                                ++crossBranches;
                                ++crossFunctionBranches;
                            }
                        }
                        if (!ref.getReferenceType().isData() || !inGuestMemory(target)) continue;
                        referencedHere.add(target.getOffset());
                        Data data = inImage(target) ?
                            currentProgram.getListing().getDefinedDataContaining(target) : null;
                        Instruction codeAt = inImage(target) ? getInstructionContaining(target) : null;
                        String kind = data != null ?
                            (data.getValue() instanceof String ? "string" : "defined_data") :
                            codeAt != null ? "instruction" :
                            inImage(target) ? "untyped_image" : "uninitialized_runtime";
                        Symbol symbol = inImage(target) ?
                            currentProgram.getSymbolTable().getPrimarySymbol(target) : null;
                        refs.write(String.format("0x%08x\t0x%08x\t0x%08x\t%s\t%s\t%s\t%s\t%s\n",
                            function.getEntryPoint().getOffset(),
                            instruction.getAddress().getOffset(), target.getOffset(),
                            ref.getReferenceType(), ref.getSource(), kind,
                            data == null ? "" : clean(data.getDataType().getName()),
                            symbol == null ? "" : clean(symbol.getName())));
                        ++dataReferences;
                        dataTargets.add(target.getOffset());
                        if (ref.getReferenceType().isRead()) ++reads;
                        if (ref.getReferenceType().isWrite()) ++writes;
                    }
                    for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
                        for (Object object : instruction.getOpObjects(operand)) {
                            Long value = null;
                            String form = "";
                            if (object instanceof Address address && globalCandidate(address)) {
                                value = address.getOffset();
                                form = "address";
                            } else if (object instanceof Scalar scalar &&
                                    scalar.getUnsignedValue() >= GLOBAL_CANDIDATE_START &&
                                    scalar.getUnsignedValue() < BSS_END) {
                                value = scalar.getUnsignedValue();
                                form = "scalar";
                            }
                            if (value == null || referencedHere.contains(value)) continue;
                            referencedHere.add(value);
                            Address target = toAddr(value);
                            Symbol symbol = value < IMAGE_END ?
                                currentProgram.getSymbolTable().getPrimarySymbol(target) : null;
                            candidates.write(String.format(
                                "0x%08x\t0x%08x\t0x%08x\t%d\t%s\t%s\t%s\n",
                                function.getEntryPoint().getOffset(),
                                instruction.getAddress().getOffset(), value, operand, form,
                                value < IMAGE_END ? "initialized" : "runtime_candidate",
                                symbol == null ? "" : clean(symbol.getName())));
                            ++addressCandidates;
                            ++literalCount;
                        }
                    }
                }
                facts.write(String.format("0x%08x\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                    function.getEntryPoint().getOffset(), clean(function.getName()),
                    function.getBody().getNumAddresses(), instructions, calls, indirect,
                    branches, loads, stores, dataTargets.size(), reads, writes, literalCount,
                    crossBranches));
                ++count;
            }
        }
        println(String.format("EF2000: %d functions, %d data references, %d address candidates, %d cross-function jumps to %s",
            count, dataReferences, addressCandidates, crossFunctionBranches, output));
    }

    private boolean inImage(Address address) {
        return address.isMemoryAddress() &&
               address.getAddressSpace().equals(toAddr(0).getAddressSpace()) &&
               address.getOffset() >= 0 && address.getOffset() < IMAGE_END;
    }

    private boolean globalCandidate(Address address) {
        return address.isMemoryAddress() &&
               address.getAddressSpace().equals(toAddr(0).getAddressSpace()) &&
               address.getOffset() >= GLOBAL_CANDIDATE_START &&
               address.getOffset() < BSS_END;
    }

    private boolean inGuestMemory(Address address) {
        return address.isMemoryAddress() &&
               address.getAddressSpace().equals(toAddr(0).getAddressSpace()) &&
               address.getOffset() >= 0 && address.getOffset() < BSS_END;
    }

    private String clean(String value) {
        return value.replace('\t', ' ').replace('\r', ' ').replace('\n', ' ');
    }
}
