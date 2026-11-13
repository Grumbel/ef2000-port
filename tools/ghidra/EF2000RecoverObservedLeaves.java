// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Data;
import ghidra.program.model.scalar.Scalar;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;
import java.util.Set;

/** Reviewed branch-free leaves exposed by checked indirect dispatch. No byte recipes. */
public class EF2000RecoverObservedLeaves extends GhidraScript {
    private static final int IMAGE_SIZE = 0x195fb0;
    // entry, exclusive end, corroborating MOV ECX literal, previous body bytes.
    // Both callbacks follow RET and precede independent caller prologues.
    // XLAT's missing flow annotation had cut the third body before its store/RET.
    private static final long[][] RECIPES = {
        {0xa432f, 0xa4342, 0xa4352, 0},
        {0xe300f, 0xe3022, 0xe3026, 0},
        {0xc64d0, 0xc64ea, 0, 20},
    };
    private static final String[] NAMES = {
        "format_buffer_putc_a432f", "format_buffer_putc_e300f", "shade_lookup_store_c64d0"
    };

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3 || (!args[1].equals("report") && !args[1].equals("apply"))) {
            throw new IllegalArgumentException("expected report-path, report|apply, payload-sha256");
        }
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default")) {
            throw new IllegalStateException("expected x86:LE:32:default");
        }
        byte[] image = new byte[IMAGE_SIZE];
        if (currentProgram.getMemory().getBytes(toAddr(0), image) != IMAGE_SIZE ||
                !HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(image)).equals(args[2])) {
            throw new IllegalStateException("program image/provenance mismatch");
        }
        List<String> rows = new ArrayList<>();
        rows.add("entry\tend_exclusive\tname\tbody_bytes_before\tbody_bytes_after\tresult");
        int transaction = currentProgram.startTransaction("reviewed observed leaf recovery");
        boolean commit = false;
        try {
            for (int index = 0; index < RECIPES.length; ++index) {
                long[] recipe = RECIPES[index];
                long entry = recipe[0], end = recipe[1], literal = recipe[2];
                Function function = getFunctionAt(toAddr(entry));
                long before = function == null ? 0 : function.getBody().getNumAddresses();
                if (before != recipe[3] && before != end-entry) {
                    throw new IllegalStateException("unexpected previous body at " + toAddr(entry));
                }
                if (literal != 0) {
                    Instruction reference = getInstructionAt(toAddr(literal));
                    boolean matches = false;
                    if (reference != null && reference.getMnemonicString().equals("MOV") &&
                            reference.getRegister(0) != null && reference.getRegister(0).getName().equals("ECX") &&
                            getFunctionContaining(toAddr(literal)) != null) {
                        for (Object object : reference.getOpObjects(1)) {
                            if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == entry) matches = true;
                            if (object instanceof Address && ((Address)object).getOffset() == entry) matches = true;
                        }
                    }
                    if (!matches) throw new IllegalStateException("missing corroborating callback literal");
                }
                for (long cursor = entry; cursor < end; ++cursor) {
                    Function owner = getFunctionContaining(toAddr(cursor));
                    Data data = currentProgram.getListing().getDefinedDataContaining(toAddr(cursor));
                    if (owner != null && owner != function) {
                        throw new IllegalStateException("owned code conflict at " + toAddr(cursor));
                    }
                    if (data != null) {
                        // The sole reviewed false datum is the absolute address
                        // operand of the MOV immediately following XLAT.
                        if (entry != 0xc64d0 || owner != null ||
                                data.getAddress().getOffset() != 0xc64e5 || data.getLength() != 4 ||
                                !(data.getValue() instanceof Address) ||
                                ((Address)data.getValue()).getOffset() != 0x159138) {
                            throw new IllegalStateException("unreviewed data conflict at " + toAddr(cursor));
                        }
                        currentProgram.getListing().clearCodeUnits(data.getAddress(), data.getMaxAddress(), false);
                    }
                }
                AddressSet body = new AddressSet(toAddr(entry), toAddr(end-1));
                long cursor = entry;
                while (cursor < end) {
                    Instruction instruction = getInstructionAt(toAddr(cursor));
                    if (instruction == null) {
                        if (getInstructionContaining(toAddr(cursor)) != null ||
                                !new DisassembleCommand(toAddr(cursor), body, false).applyTo(currentProgram, monitor)) {
                            throw new IllegalStateException("cannot decode reviewed boundary");
                        }
                        instruction = getInstructionAt(toAddr(cursor));
                    }
                    if (instruction == null || cursor + instruction.getLength() > end) {
                        throw new IllegalStateException("instruction crosses reviewed boundary");
                    }
                    String mnemonic = instruction.getMnemonicString();
                    boolean last = cursor + instruction.getLength() == end;
                    if (last ? !mnemonic.equals("RET") :
                            !Set.of("PUSH", "POP", "MOV", "LEA", "INC", "LODS", "LODSW", "ADD", "XLAT", "XLATB").contains(mnemonic)) {
                        throw new IllegalStateException("unexpected leaf control flow at " + toAddr(cursor));
                    }
                    cursor += instruction.getLength();
                }
                if (function == null) function = createFunction(toAddr(entry), NAMES[index]);
                if (function == null) throw new IllegalStateException("function creation failed");
                function.setBody(body);
                if (function.getBody().getNumAddresses() != end-entry) {
                    throw new IllegalStateException("unexpected recovered body size");
                }
                rows.add(String.format("0x%08x\t0x%08x\t%s\t%d\t%d\t%s", entry, end,
                    NAMES[index], before, end-entry, before == end-entry ? "already_recovered" :
                    args[1].equals("apply") ? "recovered" : "proposed"));
            }
            commit = args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction, commit);
        }
        var output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", rows) + "\n");
        println("EF2000: audited three observed leaf boundaries; " + args[1]);
    }
}
