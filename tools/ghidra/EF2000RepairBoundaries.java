// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.CodeUnit;
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

    private static class Recipe {
        final long entry, gap, predecessor, end, nextEntry, sync;
        final String name, precedingMnemonic, syncMnemonic;
        final int precedingLength, previousBodyLength;
        final int[] gapBytes, padding;
        final BadInstruction[] bad;

        Recipe(long entry, String name, long gap, long predecessor,
                String precedingMnemonic, int precedingLength, long end, long nextEntry,
                long sync, String syncMnemonic, int previousBodyLength,
                int[] gapBytes, int[] padding, BadInstruction[] bad) {
            this.entry = entry;
            this.name = name;
            this.gap = gap;
            this.predecessor = predecessor;
            this.precedingMnemonic = precedingMnemonic;
            this.precedingLength = precedingLength;
            this.end = end;
            this.nextEntry = nextEntry;
            this.sync = sync;
            this.syncMnemonic = syncMnemonic;
            this.previousBodyLength = previousBodyLength;
            this.gapBytes = gapBytes;
            this.padding = padding;
            this.bad = bad;
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
            0x4070, "XOR", 2, 0x40d9, 0x40dc, 0x407c, null, 34,
            new int[]{0xe8, 0xe9, 0x88, 0x09, 0x00},
            new int[]{0x8b, 0xc0},
            new BadInstruction[]{new BadInstruction(0x4074, 2, "MOV"),
                                 new BadInstruction(0x4076, 3, "ADD"),
                                 new BadInstruction(0x4079, 3, "RET")}),
        new Recipe(0x45a4, "candidate_direct_call_000045a4", 0x45d1,
            0x45cc, "CALL", 5, 0x4734, 0x4738, 0x45d6, "MOV", 45,
            new int[]{0xba, 0x28, 0x58, 0x12, 0x00},
            new int[]{0x8d, 0x40, 0x00},
            new BadInstruction[]{new BadInstruction(0x45d3, 1, "POP"),
                                 new BadInstruction(0x45d4, 2, "ADC")}),
        new Recipe(0x4738, "candidate_callback_target_00004738", 0x47ff,
            0x47fa, "CALL", 5, 0x4878, 0x487c, 0x480e, "MOV", 199,
            new int[]{0x31, 0xd2},
            new int[]{0x8d, 0x40, 0x00},
            new BadInstruction[]{new BadInstruction(0x4800, 6, "ROR"),
                                 new BadInstruction(0x4806, 3, "ADD"),
                                 new BadInstruction(0x4809, 5, "XOR")}),
        new Recipe(0x4bf8, "candidate_callback_target_00004bf8", 0x4c34,
            0x4c2f, "CALL", 5, 0x4c4e, 0x4c50, 0x4c3b, "MOV", 60,
            new int[]{0x31, 0xd2},
            new int[]{0x90},
            new BadInstruction[]{new BadInstruction(0x4c35, 6, "SAR")}),
        new Recipe(0x4e14, "candidate_callback_target_00004e14", 0x4efd,
            0x4efa, "ADD", 3, 0x4f17, 0x4f18, 0x4f0c, "XOR", 233,
            new int[]{0xe8, 0xfa, 0x75, 0x07, 0x00},
            new int[]{},
            new BadInstruction[]{new BadInstruction(0x4f01, 2, "ADD"),
                                 new BadInstruction(0x4f03, 7, "ROR"),
                                 new BadInstruction(0x4f0a, 2, "OR")}),
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
                        DisassembleCommand command = new DisassembleCommand(toAddr(recipe.gap),
                            new AddressSet(toAddr(recipe.gap), toAddr(recipe.end)), true);
                        boolean disassembled = command.applyTo(currentProgram, monitor);
                        String issue = linearIssue(recipe);
                        if (!disassembled || !expectedGap(recipe) || issue != null) {
                            throw new IllegalStateException(String.format(
                                "verified linear disassembly did not form (command=%s gap=%s issue=%s)",
                                disassembled, expectedGap(recipe), issue));
                        }
                        function.setBody(new AddressSet(toAddr(recipe.entry), toAddr(recipe.end)));
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
                function.getBody().getMaxAddress().getOffset() != recipe.gap - 1 ||
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
                readByte(recipe.nextEntry) != 0x53) return false;
        for (long offset = recipe.gap; offset <= recipe.end; offset++) {
            monitor.checkCancelled();
            if (getFunctionContaining(toAddr(offset)) != null ||
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
            default -> "UNEXPECTED";
        };
        return instruction != null && instruction.getLength() == recipe.gapBytes.length &&
               instruction.getMnemonicString().equals(mnemonic);
    }

    private boolean isLinearCode(Recipe recipe) {
        return linearIssue(recipe) == null;
    }

    private String linearIssue(Recipe recipe) {
        long offset = recipe.entry;
        while (offset <= recipe.end) {
            Instruction instruction = getInstructionAt(toAddr(offset));
            if (instruction == null) return String.format("no instruction at 0x%x", offset);
            if (instruction.getLength() <= 0 ||
                    offset + instruction.getLength() - 1 > recipe.end) {
                return String.format("bad instruction at 0x%x: %s", offset, instruction);
            }
            offset += instruction.getLength();
        }
        return offset == recipe.end + 1 &&
               getInstructionAt(toAddr(recipe.end)).getMnemonicString().equals("RET") ?
               null : "no terminating RET";
    }

    private boolean isLinearBody(Function function, Recipe recipe) {
        return function.getBody().getNumAddresses() == recipe.end - recipe.entry + 1 &&
               function.getBody().contains(toAddr(recipe.end)) && isLinearCode(recipe);
    }
}
