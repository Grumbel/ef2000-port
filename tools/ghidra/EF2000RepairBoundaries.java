// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

/** Repair only byte-verified, previously audited false orphan instruction boundaries. */
public class EF2000RepairBoundaries extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;

    private static class BadInstruction {
        final long offset;
        final int length;
        final String mnemonic;

        BadInstruction(long offset, int length, String mnemonic) {
            this.offset = offset;
            this.length = length;
            this.mnemonic = mnemonic;
        }
    }

    private static class BranchIsland {
        final long start, pointer, firstBranch, secondBranch, secondTarget;
        final int[] bytes;

        BranchIsland(long start, long pointer, long firstBranch, long secondBranch,
                     long secondTarget, int[] bytes) {
            this.start = start;
            this.pointer = pointer;
            this.firstBranch = firstBranch;
            this.secondBranch = secondBranch;
            this.secondTarget = secondTarget;
            this.bytes = bytes;
        }

        long end() { return start + bytes.length - 1; }
    }

    private static class Recipe {
        final long entry, gap, predecessor, end, nextEntry, sync, bodyResume;
        final String name, precedingMnemonic, syncMnemonic;
        final int precedingLength, previousBodyLength, nextEntryByte;
        final int[] gapBytes, padding;
        final BadInstruction[] bad;
        final long[][] oldBodyRanges;
        final long[][] targetBodyRanges;
        final BranchIsland branchIsland;

        Recipe(long entry, String name, long gap, long predecessor,
                String precedingMnemonic, int precedingLength, long end, long nextEntry,
                int nextEntryByte, long sync, String syncMnemonic, int previousBodyLength,
                int[] gapBytes, int[] padding, BadInstruction[] bad) {
            this(entry, name, gap, predecessor, precedingMnemonic, precedingLength,
                end, nextEntry, nextEntryByte, sync, syncMnemonic, previousBodyLength,
                gapBytes, padding, bad, 0);
        }

        Recipe(long entry, String name, long gap, long predecessor,
                String precedingMnemonic, int precedingLength, long end, long nextEntry,
                int nextEntryByte, long sync, String syncMnemonic, int previousBodyLength,
                int[] gapBytes, int[] padding, BadInstruction[] bad, long bodyResume) {
            this(entry, name, gap, predecessor, precedingMnemonic, precedingLength,
                end, nextEntry, nextEntryByte, sync, syncMnemonic, previousBodyLength,
                gapBytes, padding, bad, bodyResume, null, null, null);
        }

        Recipe(long entry, String name, long gap, long predecessor,
                String precedingMnemonic, int precedingLength, long end, long nextEntry,
                int nextEntryByte, long sync, String syncMnemonic, int previousBodyLength,
                int[] gapBytes, int[] padding, BadInstruction[] bad, long bodyResume,
                long[][] oldBodyRanges, BranchIsland branchIsland) {
            this(entry, name, gap, predecessor, precedingMnemonic, precedingLength,
                end, nextEntry, nextEntryByte, sync, syncMnemonic, previousBodyLength,
                gapBytes, padding, bad, bodyResume, oldBodyRanges, branchIsland, null);
        }

        Recipe(long entry, String name, long gap, long predecessor,
                String precedingMnemonic, int precedingLength, long end, long nextEntry,
                int nextEntryByte, long sync, String syncMnemonic, int previousBodyLength,
                int[] gapBytes, int[] padding, BadInstruction[] bad, long bodyResume,
                long[][] oldBodyRanges, BranchIsland branchIsland, long[][] targetBodyRanges) {
            this.entry = entry;
            this.name = name;
            this.gap = gap;
            this.predecessor = predecessor;
            this.precedingMnemonic = precedingMnemonic;
            this.precedingLength = precedingLength;
            this.end = end;
            this.nextEntry = nextEntry;
            this.nextEntryByte = nextEntryByte;
            this.sync = sync;
            this.syncMnemonic = syncMnemonic;
            this.previousBodyLength = previousBodyLength;
            this.gapBytes = gapBytes;
            this.padding = padding;
            this.bad = bad;
            this.bodyResume = bodyResume;
            this.oldBodyRanges = oldBodyRanges;
            this.branchIsland = branchIsland;
            this.targetBodyRanges = targetBodyRanges;
        }

        long badEnd() {
            BadInstruction last = bad[bad.length - 1];
            return last.offset + last.length - 1;
        }
    }

    // Each recipe is based on original-image bytes and the independently
    // identified RET / next prologue, not merely a Ghidra fall-through guess.
    // Do not add a candidate without manually checking its refs and decoding.
    private static final Recipe[] RECIPES = {
        new Recipe(0x4050, "FUN_00004050", 0x4072,
            0x4070, "XOR", 2, 0x40d9, 0x40dc, 0x53, 0x407c, null, 34,
            new int[]{0xe8, 0xe9, 0x88, 0x09, 0x00},
            new int[]{0x8b, 0xc0},
            new BadInstruction[]{new BadInstruction(0x4074, 2, "MOV"),
                                 new BadInstruction(0x4076, 3, "ADD"),
                                 new BadInstruction(0x4079, 3, "RET")}),
        new Recipe(0x433c, "FUN_0000433c", 0x4345,
            0x4342, "SUB", 3, 0x442d, 0x4430, 0x56, 0x434b, "MOV", 9,
            new int[]{0x8b, 0x2d, 0xa0, 0x70, 0x19, 0x00},
            new int[]{0x8b, 0xc0},
            new BadInstruction[]{new BadInstruction(0x4349, 2, "SBB")}),
        new Recipe(0x45a4, "candidate_direct_call_000045a4", 0x45d1,
            0x45cc, "CALL", 5, 0x4734, 0x4738, 0x53, 0x45d6, "MOV", 45,
            new int[]{0xba, 0x28, 0x58, 0x12, 0x00},
            new int[]{0x8d, 0x40, 0x00},
            new BadInstruction[]{new BadInstruction(0x45d3, 1, "POP"),
                                 new BadInstruction(0x45d4, 2, "ADC")}),
        new Recipe(0x4738, "candidate_callback_target_00004738", 0x47ff,
            0x47fa, "CALL", 5, 0x4878, 0x487c, 0x53, 0x480e, "MOV", 199,
            new int[]{0x31, 0xd2},
            new int[]{0x8d, 0x40, 0x00},
            new BadInstruction[]{new BadInstruction(0x4800, 6, "ROR"),
                                 new BadInstruction(0x4806, 3, "ADD"),
                                 new BadInstruction(0x4809, 5, "XOR")}),
        new Recipe(0x4aa0, "FUN_00004aa0", 0x4b30,
            0x4b2f, "PUSH", 1, 0x4bf4, 0x4bf8, 0x52, 0x4b35, "MOV", 144,
            new int[]{0xe8, 0x0d, 0xf8, 0x09, 0x00},
            new int[]{0x8d, 0x40, 0x00},
            new BadInstruction[]{new BadInstruction(0x4b32, 1, "CLC"),
                                 new BadInstruction(0x4b33, 2, "OR")}, 0,
            null, null, new long[][]{{0x4aa0, 0x4b50}, {0x4b7b, 0x4bf4}}),
        new Recipe(0x4bf8, "candidate_callback_target_00004bf8", 0x4c34,
            0x4c2f, "CALL", 5, 0x4c4e, 0x4c50, 0x53, 0x4c3b, "MOV", 60,
            new int[]{0x31, 0xd2},
            new int[]{0x90},
            new BadInstruction[]{new BadInstruction(0x4c35, 6, "SAR")}),
        new Recipe(0x4e14, "candidate_callback_target_00004e14", 0x4efd,
            0x4efa, "ADD", 3, 0x4f17, 0x4f18, 0x53, 0x4f0c, "XOR", 233,
            new int[]{0xe8, 0xfa, 0x75, 0x07, 0x00},
            new int[]{},
            new BadInstruction[]{new BadInstruction(0x4f01, 2, "ADD"),
                                 new BadInstruction(0x4f03, 7, "ROR"),
                                 new BadInstruction(0x4f0a, 2, "OR")}),
        new Recipe(0x9820, "FUN_00009820", 0x9c1f,
            0x9c1a, "CALL", 5, 0x9e37, 0x9e38, 0x56, 0x9c2e, "JLE", 1021,
            new int[]{0x8b, 0x15, 0x58, 0x79, 0x13, 0x00},
            new int[]{},
            new BadInstruction[]{new BadInstruction(0x9c24, 6, "ADD"),
                                 new BadInstruction(0x9c2a, 4, "CMP")}, 0x9e2e,
            new long[][]{{0x9820, 0x9bba}, {0x9bc7, 0x9c1e}, {0x9e2e, 0x9e37}},
            new BranchIsland(0x9bbb, 0x9bbc, 0x9ba9, 0x9bb9, 0x9bc0,
                new int[]{0xba, 0xab, 0xf6, 0x12, 0x00, 0x89, 0xf0,
                          0xe8, 0xc1, 0xd9, 0x09, 0x00})),
        new Recipe(0xfecc, "FUN_0000fecc", 0xfefe,
            0xfefd, "PUSH", 1, 0xffb2, 0xffb4, 0x53, 0xff09, "ADD", 217,
            new int[]{0x68, 0x74, 0x11, 0x18, 0x00},
            new int[]{0x90},
            new BadInstruction[]{new BadInstruction(0xff00, 2, "ADC"),
                                 new BadInstruction(0xff02, 2, "ADD"),
                                 new BadInstruction(0xff04, 5, "ADC")}, 0xff0c),
    };

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2 || (!args[1].equals("report") && !args[1].equals("apply"))) {
            throw new IllegalArgumentException("expected report.tsv report|apply");
        }
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(0)) == null ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected pinned zero-based EF2000 X-32 image");
        }
        boolean apply = args[1].equals("apply");
        List<String> rows = new ArrayList<>();
        rows.add("entry\tgap\tfirst_bad\treturn\tbody_bytes_before\tbody_bytes_after\tresult\terror");
        for (Recipe recipe : RECIPES) {
            monitor.checkCancelled();
            Function function = getFunctionAt(toAddr(recipe.entry));
            long before = function == null ? -1 : function.getBody().getNumAddresses();
            String result = "ineligible", error = "";
            if (function != null && isLinearBody(function, recipe)) {
                result = "already_repaired";
            } else if (safeToRepair(function, recipe)) {
                result = "proposed";
                if (apply) {
                    int transaction = currentProgram.startTransaction(
                        String.format("Repair verified EF2000 boundary 0x%x", recipe.entry));
                    boolean commit = false;
                    try {
                        // Clear only the exact wrong orphan instruction span;
                        // never erase a discovered function or arbitrary code.
                        currentProgram.getListing().clearCodeUnits(
                            toAddr(recipe.bad[0].offset), toAddr(recipe.badEnd()), false);
                        if (recipe.branchIsland != null) {
                            BranchIsland island = recipe.branchIsland;
                            currentProgram.getListing().clearCodeUnits(
                                toAddr(island.pointer), toAddr(island.pointer + 3), false);
                            DisassembleCommand branch = new DisassembleCommand(toAddr(island.start),
                                new AddressSet(toAddr(island.start), toAddr(island.end())), true);
                            if (!branch.applyTo(currentProgram, monitor)) {
                                throw new IllegalStateException("verified branch island did not disassemble");
                            }
                        }
                        AddressSet permitted = recipe.targetBodyRanges == null ?
                            new AddressSet(toAddr(recipe.gap), toAddr(recipe.end)) :
                            bodyAddressSet(recipe.targetBodyRanges);
                        DisassembleCommand command = new DisassembleCommand(toAddr(recipe.gap),
                            permitted, true);
                        boolean disassembled = command.applyTo(currentProgram, monitor);
                        String issue = linearIssue(recipe);
                        if (!disassembled || !expectedGap(recipe) || issue != null) {
                            throw new IllegalStateException(String.format(
                                "verified linear disassembly did not form (command=%s gap=%s issue=%s)",
                                disassembled, expectedGap(recipe), issue));
                        }
                        function.setBody(recipe.targetBodyRanges == null ?
                            new AddressSet(toAddr(recipe.entry), toAddr(recipe.end)) :
                            bodyAddressSet(recipe.targetBodyRanges));
                        if (!isLinearBody(function, recipe)) {
                            throw new IllegalStateException("function body did not extend to RET");
                        }
                        result = "repaired";
                        commit = true;
                    } catch (Exception exception) {
                        result = "repair_failed";
                        error = exception.getMessage() == null ?
                            exception.toString() : exception.getMessage();
                    } finally {
                        currentProgram.endTransaction(transaction, commit);
                    }
                }
            }
            Function after = getFunctionAt(toAddr(recipe.entry));
            long size = after == null ? -1 : after.getBody().getNumAddresses();
            rows.add(String.format("0x%08x\t0x%08x\t0x%08x\t0x%08x\t%d\t%d\t%s\t%s",
                recipe.entry, recipe.gap, recipe.bad[0].offset, recipe.end,
                before, size, result, error.replace('\n', ' ').replace('\t', ' ')));
            println(String.format("EF2000: boundary 0x%x %s, body %d -> %d bytes",
                recipe.entry, result, before, size));
        }
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", rows) + "\n", StandardCharsets.UTF_8);
        println("EF2000: wrote " + output);
    }

    private boolean safeToRepair(Function function, Recipe recipe) throws Exception {
        if (function == null || function.isThunk() ||
                !function.getName().equals(recipe.name) ||
                function.getBody().getNumAddresses() != recipe.previousBodyLength ||
                !matchesOldBody(function, recipe) ||
                getFunctionAt(toAddr(recipe.nextEntry)) == null ||
                getInstructionAt(toAddr(recipe.gap)) != null) return false;
        Instruction predecessor = getInstructionAt(toAddr(recipe.predecessor));
        Instruction end = getInstructionAt(toAddr(recipe.end));
        Instruction sync = getInstructionAt(toAddr(recipe.sync));
        boolean endMatches = end != null && end.getMnemonicString().equals("RET") &&
                             end.getLength() == 1;
        // This one continuation was left entirely undefined by Ghidra after
        // the spurious RET 0x30be; its real final C3 is verified in raw bytes.
        if (recipe.entry == 0x4050 && end == null &&
                getInstructionContaining(toAddr(recipe.end)) == null &&
                readByte(recipe.end) == 0xc3) endMatches = true;
        if (predecessor == null ||
                !predecessor.getMnemonicString().equals(recipe.precedingMnemonic) ||
                predecessor.getLength() != recipe.precedingLength ||
                recipe.predecessor + predecessor.getLength() != recipe.gap ||
                !endMatches ||
                (recipe.syncMnemonic == null ? sync != null :
                    sync == null || !sync.getMnemonicString().equals(recipe.syncMnemonic)) ||
                recipe.badEnd() + 1 != recipe.sync) return false;
        if (!bytesMatch(recipe.gap, recipe.gapBytes) ||
                !bytesMatch(recipe.end + 1, recipe.padding) ||
                readByte(recipe.nextEntry) != recipe.nextEntryByte) return false;
        if (recipe.targetBodyRanges != null && !safeExcludedNeighbor(recipe)) return false;
        if (recipe.branchIsland != null && !safeBranchIsland(recipe.branchIsland)) return false;
        for (long offset = recipe.gap; offset <= recipe.end; offset++) {
            monitor.checkCancelled();
            if (recipe.targetBodyRanges != null &&
                    !inBodyRanges(recipe.targetBodyRanges, offset)) continue;
            Function owner = getFunctionContaining(toAddr(offset));
            if ((owner != null && (owner != function ||
                    recipe.bodyResume == 0 || offset < recipe.bodyResume)) ||
                    currentProgram.getListing().getDefinedDataContaining(toAddr(offset)) != null) return false;
        }
        // Entire cleared span must be *exactly* the audited orphan instructions.
        // A default/user data reference, control-flow edge, symbol or comment
        // blocks repair even if the raw bytes otherwise appear correct.
        long nextBad = recipe.bad[0].offset;
        for (BadInstruction bad : recipe.bad) {
            if (bad.offset != nextBad) return false;
            Instruction instruction = getInstructionAt(toAddr(bad.offset));
            if (instruction == null || instruction.getLength() != bad.length ||
                    !instruction.getMnemonicString().equals(bad.mnemonic)) return false;
            for (long offset = bad.offset; offset < bad.offset + bad.length; offset++) {
                if (getInstructionContaining(toAddr(offset)) != instruction) return false;
                for (Reference reference : getReferencesTo(toAddr(offset))) {
                    if (!reference.getReferenceType().isData() ||
                            reference.getSource() != SourceType.ANALYSIS) return false;
                }
                for (Symbol symbol : currentProgram.getSymbolTable().getSymbols(toAddr(offset))) {
                    if (symbol.getSource() == SourceType.USER_DEFINED) return false;
                }
            }
            for (int type : new int[]{CodeUnit.EOL_COMMENT, CodeUnit.PRE_COMMENT,
                                      CodeUnit.POST_COMMENT, CodeUnit.PLATE_COMMENT,
                                      CodeUnit.REPEATABLE_COMMENT}) {
                if (instruction.getComment(type) != null) return false;
            }
            nextBad += bad.length;
        }
        return true;
    }

    private int readByte(long offset) throws Exception {
        return currentProgram.getMemory().getByte(toAddr(offset)) & 0xff;
    }

    private boolean matchesOldBody(Function function, Recipe recipe) {
        if (recipe.oldBodyRanges != null) {
            if (function.getBody().getNumAddressRanges() != recipe.oldBodyRanges.length) return false;
            var ranges = function.getBody().getAddressRanges();
            for (long[] span : recipe.oldBodyRanges) {
                var range = ranges.next();
                if (range.getMinAddress().getOffset() != span[0] ||
                        range.getMaxAddress().getOffset() != span[1]) return false;
            }
            return true;
        }
        if (recipe.bodyResume == 0) {
            return function.getBody().getMaxAddress().getOffset() == recipe.gap - 1 &&
                   function.getBody().getNumAddressRanges() == 1;
        }
        // Exactly two old ranges, separated by the single verified bad
        // decode. Existing owned instructions after the hole must be kept.
        if (function.getBody().getNumAddressRanges() != 2 ||
                function.getBody().getMaxAddress().getOffset() != recipe.end ||
                recipe.bodyResume <= recipe.gap) return false;
        for (long offset = recipe.entry; offset <= recipe.end; offset++) {
            boolean shouldOwn = offset < recipe.gap || offset >= recipe.bodyResume;
            if (function.getBody().contains(toAddr(offset)) != shouldOwn) return false;
        }
        return true;
    }

    private boolean safeExcludedNeighbor(Recipe recipe) throws Exception {
        // 0x4b54 is a separate table-backed callback reached from elsewhere.
        // Its 39 bytes and the preceding 3-byte alignment must remain outside
        // the 0x4aa0 function, despite physically occupying its branch shadow.
        if (recipe.entry != 0x4aa0 || recipe.targetBodyRanges.length != 2 ||
                !inBodyRanges(recipe.targetBodyRanges, recipe.entry) ||
                inBodyRanges(recipe.targetBodyRanges, 0x4b54) ||
                !bytesMatch(0x4b51, new int[]{0x8d, 0x40, 0x00})) return false;
        Function other = getFunctionAt(toAddr(0x4b54));
        return other != null && other.getName().equals("kotsInitKOTS") &&
               other.getBody().getNumAddressRanges() == 1 &&
               other.getBody().getNumAddresses() == 39 &&
               other.getBody().getMinAddress().getOffset() == 0x4b54 &&
               other.getBody().getMaxAddress().getOffset() == 0x4b7a;
    }

    private boolean inBodyRanges(long[][] ranges, long offset) {
        for (long[] span : ranges) {
            if (offset >= span[0] && offset <= span[1]) return true;
        }
        return false;
    }

    private AddressSet bodyAddressSet(long[][] ranges) {
        AddressSet body = new AddressSet();
        for (long[] span : ranges) body.addRange(toAddr(span[0]), toAddr(span[1]));
        return body;
    }

    private boolean safeBranchIsland(BranchIsland island) throws Exception {
        if (!bytesMatch(island.start, island.bytes)) return false;
        Data pointer = currentProgram.getListing().getDefinedDataAt(toAddr(island.pointer));
        if (pointer == null || pointer.getLength() != 4 ||
                !pointer.getDataType().getName().equals("pointer")) return false;
        for (long offset = island.start; offset <= island.end(); offset++) {
            monitor.checkCancelled();
            if (getInstructionContaining(toAddr(offset)) != null ||
                    getFunctionContaining(toAddr(offset)) != null) return false;
            Data data = currentProgram.getListing().getDefinedDataContaining(toAddr(offset));
            if (data != null && data.getAddress().getOffset() != island.pointer) return false;
            if (data != null && (offset < island.pointer || offset >= island.pointer + 4)) return false;
            CodeUnit unit = currentProgram.getListing().getCodeUnitContaining(toAddr(offset));
            if (unit != null && unit.getAddress().getOffset() == offset) {
                for (int type : new int[]{CodeUnit.EOL_COMMENT, CodeUnit.PRE_COMMENT,
                                          CodeUnit.POST_COMMENT, CodeUnit.PLATE_COMMENT,
                                          CodeUnit.REPEATABLE_COMMENT}) {
                    if (unit.getComment(type) != null) return false;
                }
            }
            for (Reference reference : getReferencesTo(toAddr(offset))) {
                boolean verifiedBranch = (offset == island.start &&
                    reference.getFromAddress().getOffset() == island.firstBranch) ||
                    (offset == island.secondTarget &&
                    reference.getFromAddress().getOffset() == island.secondBranch);
                if (!verifiedBranch && (!reference.getReferenceType().isData() ||
                        reference.getSource() != SourceType.ANALYSIS)) return false;
            }
            for (Symbol symbol : currentProgram.getSymbolTable().getSymbols(toAddr(offset))) {
                if (symbol.getSource() == SourceType.USER_DEFINED) return false;
            }
        }
        return hasBranchReference(island.start, island.firstBranch) &&
               hasBranchReference(island.secondTarget, island.secondBranch);
    }

    private boolean hasBranchReference(long target, long source) {
        for (Reference reference : getReferencesTo(toAddr(target))) {
            if (reference.getFromAddress().getOffset() == source &&
                    reference.getReferenceType().isJump() &&
                    reference.getSource() == SourceType.DEFAULT) return true;
        }
        return false;
    }

    private boolean bytesMatch(long start, int[] bytes) throws Exception {
        for (int i = 0; i < bytes.length; i++) {
            if (readByte(start + i) != bytes[i]) return false;
        }
        return true;
    }

    private boolean expectedGap(Recipe recipe) {
        Instruction instruction = getInstructionAt(toAddr(recipe.gap));
        String mnemonic = switch (recipe.gapBytes[0]) {
            case 0xba -> "MOV";
            case 0x31 -> "XOR";
            case 0xe8 -> "CALL";
            case 0x8b -> "MOV";
            case 0x68 -> "PUSH";
            default -> "UNEXPECTED";
        };
        return instruction != null && instruction.getLength() == recipe.gapBytes.length &&
               instruction.getMnemonicString().equals(mnemonic);
    }

    private boolean isLinearCode(Recipe recipe) {
        return linearIssue(recipe) == null;
    }

    private String linearIssue(Recipe recipe) {
        long[][] ranges = recipe.targetBodyRanges == null ?
            new long[][]{{recipe.entry, recipe.end}} : recipe.targetBodyRanges;
        for (long[] span : ranges) {
            long offset = span[0];
            while (offset <= span[1]) {
                Instruction instruction = getInstructionAt(toAddr(offset));
                if (instruction == null) return String.format("no instruction at 0x%x", offset);
                if (instruction.getLength() <= 0 ||
                        offset + instruction.getLength() - 1 > span[1]) {
                    return String.format("bad instruction at 0x%x: %s", offset, instruction);
                }
                offset += instruction.getLength();
            }
        }
        return getInstructionAt(toAddr(recipe.end)).getMnemonicString().equals("RET") ?
               null : "no terminating RET";
    }

    private boolean isLinearBody(Function function, Recipe recipe) {
        if (recipe.targetBodyRanges != null) {
            if (function.getBody().getNumAddressRanges() != recipe.targetBodyRanges.length) return false;
            var iter = function.getBody().getAddressRanges();
            for (long[] span : recipe.targetBodyRanges) {
                var range = iter.next();
                if (range.getMinAddress().getOffset() != span[0] ||
                        range.getMaxAddress().getOffset() != span[1]) return false;
            }
            return isLinearCode(recipe);
        }
        return function.getBody().getNumAddresses() == recipe.end - recipe.entry + 1 &&
               function.getBody().contains(toAddr(recipe.end)) && isLinearCode(recipe);
    }
}
