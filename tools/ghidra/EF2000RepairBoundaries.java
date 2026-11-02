// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
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

/** Repair a verified false orphan decode inside one independently reached routine. */
public class EF2000RepairBoundaries extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;
    private static final long ENTRY = 0x45a4L;
    private static final long GAP = 0x45d1L;
    private static final long BAD_START = 0x45d3L;
    private static final long BAD_END = 0x45d5L;
    private static final long RETURN = 0x4734L;
    private static final long NEXT_ENTRY = 0x4738L;

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
        Function function = getFunctionAt(toAddr(ENTRY));
        long before = function == null ? -1 : function.getBody().getNumAddresses();
        String result = "ineligible", error = "";
        if (function != null && function.getBody().contains(toAddr(RETURN)) &&
                isLinearBody(function)) {
            result = "already_repaired";
        } else if (safeToRepair(function)) {
            result = "proposed";
            if (apply) {
                int transaction = currentProgram.startTransaction("Repair verified EF2000 arcade boundary");
                boolean commit = false;
                try {
                    // Clear only two demonstrably bogus orphan instructions:
                    // POP EAX at 45d3 and ADC AL,[EAX] at 45d4. Their bytes
                    // overlap MOV EDX,0x125828 beginning at 45d1.
                    currentProgram.getListing().clearCodeUnits(
                        toAddr(BAD_START), toAddr(BAD_END), false);
                    DisassembleCommand command = new DisassembleCommand(toAddr(GAP),
                        new AddressSet(toAddr(GAP), toAddr(RETURN)), true);
                    if (!command.applyTo(currentProgram, monitor) ||
                            !isExpectedMov() || !isLinearCode()) {
                        throw new IllegalStateException("verified linear disassembly did not form");
                    }
                    function.setBody(new AddressSet(toAddr(ENTRY), toAddr(RETURN)));
                    if (!isLinearBody(function)) {
                        throw new IllegalStateException("function body did not extend to RET");
                    }
                    result = "repaired";
                    commit = true;
                } catch (Exception exception) {
                    result = "repair_failed";
                    error = exception.getMessage() == null ? exception.toString() : exception.getMessage();
                } finally {
                    currentProgram.endTransaction(transaction, commit);
                }
            }
        }
        Function after = getFunctionAt(toAddr(ENTRY));
        long size = after == null ? -1 : after.getBody().getNumAddresses();
        Path output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        String header = "entry\tgap\tfirst_bad\treturn\tbody_bytes_before\tbody_bytes_after\tresult\terror\n";
        String row = String.format("0x%08x\t0x%08x\t0x%08x\t0x%08x\t%d\t%d\t%s\t%s\n",
            ENTRY, GAP, BAD_START, RETURN, before, size, result,
            error.replace('\n', ' ').replace('\t', ' '));
        Files.writeString(output, header + row, StandardCharsets.UTF_8);
        println("EF2000: arcade boundary " + result + ", body " + before + " -> " + size +
                " bytes; wrote " + output);
    }

    private boolean safeToRepair(Function function) throws Exception {
        if (function == null || function.isThunk() ||
                !function.getName().equals("candidate_direct_call_000045a4") ||
                function.getBody().getNumAddresses() != 45 ||
                function.getBody().getMaxAddress().getOffset() != GAP - 1 ||
                getFunctionAt(toAddr(NEXT_ENTRY)) == null ||
                getInstructionAt(toAddr(GAP)) != null) return false;
        Instruction call = getInstructionAt(toAddr(0x45cc));
        Instruction bad1 = getInstructionAt(toAddr(BAD_START));
        Instruction bad2 = getInstructionAt(toAddr(BAD_START + 1));
        Instruction end = getInstructionAt(toAddr(RETURN));
        if (call == null || !call.getMnemonicString().equals("CALL") || call.getLength() != 5 ||
                bad1 == null || !bad1.getMnemonicString().equals("POP") || bad1.getLength() != 1 ||
                bad2 == null || !bad2.getMnemonicString().equals("ADC") || bad2.getLength() != 2 ||
                end == null || !end.getMnemonicString().equals("RET") || end.getLength() != 1) {
            return false;
        }
        if (readByte(GAP) != 0xba || readByte(GAP + 1) != 0x28 ||
                readByte(GAP + 2) != 0x58 || readByte(GAP + 3) != 0x12 ||
                readByte(GAP + 4) != 0x00 || readByte(NEXT_ENTRY) != 0x53 ||
                readByte(RETURN + 1) != 0x8d || readByte(RETURN + 2) != 0x40 ||
                readByte(RETURN + 3) != 0x00) return false;
        for (long offset = GAP; offset <= RETURN; ++offset) {
            monitor.checkCancelled();
            if (getFunctionContaining(toAddr(offset)) != null ||
                    currentProgram.getListing().getDefinedDataContaining(toAddr(offset)) != null) return false;
        }
        // Only a stale Ghidra ANALYSIS data reference may point into the
        // middle of the immediate (0xd8f00 -> 0x45d3 in the pinned image).
        // Never erase code-flow references, user/import data references,
        // user-defined symbols, or comments.
        for (long offset = BAD_START; offset <= BAD_START + 1; ++offset) {
            for (Reference reference : getReferencesTo(toAddr(offset))) {
                if (!reference.getReferenceType().isData() ||
                        reference.getSource() != SourceType.ANALYSIS) return false;
            }
            for (Symbol symbol : currentProgram.getSymbolTable().getSymbols(toAddr(offset))) {
                if (symbol.getSource() == SourceType.USER_DEFINED) return false;
            }
            Instruction instruction = getInstructionAt(toAddr(offset));
            if (instruction == null) return false;
            for (int type : new int[]{CodeUnit.EOL_COMMENT, CodeUnit.PRE_COMMENT,
                                      CodeUnit.POST_COMMENT, CodeUnit.PLATE_COMMENT,
                                      CodeUnit.REPEATABLE_COMMENT}) {
                if (instruction.getComment(type) != null) return false;
            }
        }
        return true;
    }

    private int readByte(long offset) throws Exception {
        return currentProgram.getMemory().getByte(toAddr(offset)) & 0xff;
    }

    private boolean isExpectedMov() {
        Instruction mov = getInstructionAt(toAddr(GAP));
        return mov != null && mov.getLength() == 5 && mov.getMnemonicString().equals("MOV");
    }

    private boolean isLinearCode() {
        long offset = ENTRY;
        while (offset <= RETURN) {
            Instruction instruction = getInstructionAt(toAddr(offset));
            if (instruction == null || instruction.getLength() <= 0 ||
                    offset + instruction.getLength() - 1 > RETURN) return false;
            offset += instruction.getLength();
        }
        return offset == RETURN + 1 &&
               getInstructionAt(toAddr(RETURN)).getMnemonicString().equals("RET");
    }

    private boolean isLinearBody(Function function) {
        return function.getBody().getNumAddresses() == RETURN - ENTRY + 1 &&
               function.getBody().contains(toAddr(RETURN)) && isLinearCode();
    }
}
