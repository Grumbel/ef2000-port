// SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
// SPDX-License-Identifier: GPL-3.0-or-later
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Name auto-discovered functions from independently corroborated guest callback tables. */
public class EF2000NameCallbacks extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 5 || (!args[2].equals("report") && !args[2].equals("apply")) ||
                (!args[3].equals("names") && !args[3].equals("thunks") &&
                 !args[3].equals("entries") && !args[3].equals("undisassembled")) ||
                (args[2].equals("report") && !args[3].equals("names"))) {
            throw new IllegalArgumentException(
                "expected callback-tables.tsv, report.tsv, report|apply, names|thunks|entries|undisassembled, max");
        }
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                currentProgram.getMemory().getBlock(toAddr(IMAGE_END - 1)) == null) {
            throw new IllegalStateException("expected zero-based EF2000 X-32 image");
        }
        boolean apply = args[2].equals("apply");
        boolean promoteThunks = args[3].equals("thunks");
        boolean promoteEntries = args[3].equals("entries");
        boolean promoteUndisassembled = args[3].equals("undisassembled");
        int maximum = Integer.parseInt(args[4]);
        if (maximum < 0 || (maximum > 0 && !promoteThunks && !promoteEntries &&
                !promoteUndisassembled)) {
            throw new IllegalArgumentException("invalid maximum promotions");
        }
        Path output = Paths.get(args[1]);
        List<String> lines = Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8);
        if (lines.isEmpty() || !lines.get(0).equals(
                "table_start\tslot\thandler\tstring_ptr\tname\tknown_function")) {
            throw new IllegalArgumentException("unexpected callback evidence columns");
        }
        List<Row> rows = new ArrayList<>();
        Map<Long, Set<String>> namesByHandler = new HashMap<>();
        Map<String, Set<Long>> handlersByName = new HashMap<>();
        for (int index = 1; index < lines.size(); ++index) {
            String[] parts = lines.get(index).split("\t", -1);
            if (parts.length != 6 || !parts[4].matches("[A-Za-z_][A-Za-z0-9_]{2,63}") ||
                    !(parts[5].equals("yes") || parts[5].equals("no"))) {
                throw new IllegalArgumentException("malformed callback evidence at line " + (index + 1));
            }
            long start = Long.decode(parts[0]);
            long slot = Long.decode(parts[1]);
            long handler = Long.decode(parts[2]);
            long stringPtr = Long.decode(parts[3]);
            if (start < 0 || slot < start || slot >= IMAGE_END - 8 ||
                    handler < 0 || handler >= 0x110000 ||
                    stringPtr < 0x170000 || stringPtr >= IMAGE_END ||
                    (currentProgram.getMemory().getInt(toAddr(slot)) & 0xffffffffL) != handler ||
                    (currentProgram.getMemory().getInt(toAddr(slot + 4)) & 0xffffffffL) != stringPtr ||
                    !readName(stringPtr).equals(parts[4])) {
                throw new IllegalArgumentException("callback evidence/image mismatch at line " + (index + 1));
            }
            Row row = new Row(start, slot, handler, parts[4]);
            rows.add(row);
            namesByHandler.computeIfAbsent(handler, key -> new HashSet<>()).add(row.name);
            handlersByName.computeIfAbsent(row.name, key -> new HashSet<>()).add(handler);
        }

        List<String> report = new ArrayList<>();
        report.add("table_start\tslot\thandler\tname\tresult\tcurrent_name");
        int renamed = 0, promoted = 0;
        for (Row row : rows) {
            monitor.checkCancelled();
            String result;
            String currentName = "";
            Function function = row.handler == 0 ? null : getFunctionAt(toAddr(row.handler));
            if (row.handler == 0) {
                result = "null_handler";
            } else if (namesByHandler.get(row.handler).size() > 1 ||
                    handlersByName.get(row.name).size() > 1) {
                result = "ambiguous_alias";
            } else {
                boolean created = false;
                boolean limited = false;
                if (function == null && ((promoteThunks && isSafeDirectJumpStub(row.handler)) ||
                        (promoteEntries && isSafeReferencedEntry(row.handler, row.slot)) ||
                        (promoteUndisassembled && isSafeUndisassembledEntry(row.handler, row.slot)))) {
                    if (maximum > 0 && promoted >= maximum) {
                        limited = true;
                    } else {
                        try {
                            boolean disassembled = !promoteUndisassembled ||
                                disassembleCallback(row.handler);
                            if (disassembled) {
                                function = createFunction(toAddr(row.handler),
                                    String.format("candidate_callback_%08x", row.handler));
                            }
                            if (function != null) {
                                created = true;
                                ++promoted;
                            }
                        } catch (Exception error) {
                            // Preserve unrelated annotations if creation fails.
                        }
                    }
                }
                if (function == null) {
                    result = limited ? "promotion_limited" : "no_function_at_handler";
                } else {
                    currentName = function.getName();
                    if (currentName.equals(row.name)) {
                        result = "already_named";
                    } else if (!currentName.matches(
                            "(thunk_)?FUN_[0-9a-fA-F]+|(thunk_)?candidate_orphan_[0-9a-fA-F]+|"
                            + "(thunk_)?candidate_callback_(thunk_)?[0-9a-fA-F]+") &&
                            !(promoteUndisassembled && function.isThunk() &&
                              currentName.matches("candidate_callback_target_[0-9a-fA-F]+"))) {
                        result = "preserved_existing_name";
                    } else if (!apply) {
                        result = "proposed";
                    } else {
                        try {
                            function.setName(row.name, SourceType.USER_DEFINED);
                            result = created ? (promoteUndisassembled ? "created_named_undisassembled" :
                                                promoteEntries ? "created_named_entry" :
                                                "created_named_thunk") : "renamed";
                            ++renamed;
                        } catch (Exception error) {
                            result = created ? (promoteUndisassembled ? "created_unnamed_undisassembled" :
                                                promoteEntries ? "created_unnamed_entry" :
                                                "created_unnamed_thunk") : "rename_failed";
                        }
                    }
                }
            }
            report.add(String.format("0x%08x\t0x%08x\t0x%08x\t%s\t%s\t%s",
                row.start, row.slot, row.handler, row.name, result, currentName));
        }
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", report) + "\n", StandardCharsets.UTF_8);
        println("EF2000: " + rows.size() + " callback table slots, " + renamed +
                " functions named, " + promoted + " functions promoted; wrote " + output);
    }

    private boolean isSafeReferencedEntry(long entry, long slot) {
        // A table pointer alone can be data. Require Ghidra to have already
        // decoded an instruction at the exact aligned address, with an exact
        // DATA xref from the independently verified name-table slot.
        if ((entry & 3) != 0 || getFunctionContaining(toAddr(entry)) != null ||
                getInstructionAt(toAddr(entry)) == null) return false;
        return hasExactDataReference(entry, slot);
    }

    private boolean hasExactDataReference(long entry, long slot) {
        for (Reference reference : getReferencesTo(toAddr(entry))) {
            if (reference.getFromAddress().getOffset() == slot &&
                    reference.getReferenceType().isData()) return true;
        }
        return false;
    }

    private boolean isSafeUndisassembledEntry(long entry, long slot) throws Exception {
        // This is deliberately narrower than disassembly of arbitrary data:
        // the named callback table was independently verified against the
        // pinned executable AND Ghidra must reference precisely this slot.
        // No existing code or typed data may be split/overwritten.
        if ((entry & 3) != 0 || entry < 0x100 || entry + 5 >= 0x110000 ||
                getFunctionContaining(toAddr(entry)) != null ||
                getInstructionContaining(toAddr(entry)) != null ||
                currentProgram.getListing().getDefinedDataContaining(toAddr(entry)) != null ||
                !hasExactDataReference(entry, slot)) return false;
        int opcode = currentProgram.getMemory().getByte(toAddr(entry)) & 0xff;
        if (opcode == 0xe9) {
            long destination = entry + 5 + currentProgram.getMemory().getInt(toAddr(entry + 1));
            if (destination < 0x100 || destination >= 0x110000 ||
                    (destination & 3) != 0) return false;
            if (getInstructionAt(toAddr(destination)) != null) return true;
            // Some real thunks point to *another* unclassified routine. In
            // this tier only decode that target if it is aligned, untouched,
            // and begins with a Watcom-style saved-register prologue.
            return getInstructionContaining(toAddr(destination)) == null &&
                   getFunctionContaining(toAddr(destination)) == null &&
                   currentProgram.getListing().getDefinedDataContaining(toAddr(destination)) == null &&
                   isSavedRegisterStart(destination);
        }
        // These are the observed Watcom-style entries, not a general x86
        // entry detector: PUSH EBX, PUSH EDX, MOV r32,r/m32.
        return opcode == 0x53 || opcode == 0x52 || opcode == 0x8b;
    }

    private boolean disassembleCallback(long entry) throws Exception {
        if ((currentProgram.getMemory().getByte(toAddr(entry)) & 0xff) == 0xe9) {
            long destination = entry + 5 + currentProgram.getMemory().getInt(toAddr(entry + 1));
            if (getInstructionAt(toAddr(destination)) == null &&
                    !disassembleBounded(destination)) return false;
            if (getFunctionContaining(toAddr(destination)) == null) {
                Function target = createFunction(toAddr(destination),
                    String.format("candidate_callback_target_%08x", destination));
                if (target == null) return false;
            }
        }
        return disassembleBounded(entry);
    }

    private boolean isSavedRegisterStart(long entry) throws Exception {
        int opcode = currentProgram.getMemory().getByte(toAddr(entry)) & 0xff;
        return opcode == 0x53 || opcode == 0x52 || opcode == 0x55 ||
               opcode == 0x56 || opcode == 0x57;
    }

    private boolean disassembleBounded(long entry) throws Exception {
        // Stop before the next already-decoded instruction. An explicit cap
        // prevents accidentally walking large data areas on a wrong entry.
        Instruction next = getInstructionAfter(toAddr(entry));
        long end = Math.min(IMAGE_END, entry + 1024);
        if (next != null) end = Math.min(end, next.getAddress().getOffset());
        if (end <= entry) return false;
        DisassembleCommand command = new DisassembleCommand(toAddr(entry),
            new AddressSet(toAddr(entry), toAddr(end - 1)), true);
        return command.applyTo(currentProgram, monitor) && getInstructionAt(toAddr(entry)) != null;
    }

    private boolean isSafeDirectJumpStub(long entry) throws Exception {
        // A five-byte E9 rel32 thunk has no fallthrough. Its destination must
        // already be part of a recognized function; never seed arbitrary data.
        if (entry < 0 || entry + 5 >= IMAGE_END ||
                getFunctionContaining(toAddr(entry)) != null ||
                (currentProgram.getMemory().getByte(toAddr(entry)) & 0xff) != 0xe9) {
            return false;
        }
        Instruction instruction = currentProgram.getListing().getInstructionAt(toAddr(entry));
        if (instruction == null || instruction.getLength() != 5 ||
                !instruction.getMnemonicString().equals("JMP")) {
            return false;
        }
        long target = entry + 5 + currentProgram.getMemory().getInt(toAddr(entry + 1));
        return target >= 0 && target < IMAGE_END &&
               getFunctionContaining(toAddr(target)) != null;
    }

    private String readName(long address) throws Exception {
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < 65 && address + i < IMAGE_END; ++i) {
            int value = currentProgram.getMemory().getByte(toAddr(address + i)) & 0xff;
            if (value == 0) return result.toString();
            if (value > 127) return "";
            result.append((char) value);
        }
        return "";
    }

    private static class Row {
        final long start, slot, handler;
        final String name;

        Row(long start, long slot, long handler, String name) {
            this.start = start;
            this.slot = slot;
            this.handler = handler;
            this.name = name;
        }
    }
}
