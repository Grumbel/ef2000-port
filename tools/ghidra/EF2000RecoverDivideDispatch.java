// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;
import java.util.Set;

/** Recover the whole reviewed 64-way division dispatch, including trap cases. */
public class EF2000RecoverDivideDispatch extends GhidraScript {
    private static final long ENTRY = 0x11f7ef, END = 0x11fc1b;
    private static final long TABLE = 0x195eb0, HELPER = 0x11f6d8, JUMP = 0x11f7f2;
    private static final int IMAGE_SIZE = 0x195fb0;

    private String hash(long start, int length) throws Exception {
        byte[] bytes = new byte[length];
        if (currentProgram.getMemory().getBytes(toAddr(start), bytes) != length)
            throw new IllegalStateException("short image read");
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 3 || (!args[1].equals("report") && !args[1].equals("apply")))
            throw new IllegalArgumentException("expected output.tsv, report|apply, payload-sha256");
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
                !hash(0, IMAGE_SIZE).equals(args[2]) ||
                !hash(ENTRY, (int)(END-ENTRY)).equals("d313ef918078053ee9ffd29b707eb7693db7af41d01179b0d60c3a47da116c47") ||
                !hash(TABLE, 256).equals("8bf591c0a085b7e5f479de26dce123673175ebc1b73c1424743910eab025a684"))
            throw new IllegalStateException("unexpected language/image/dispatch/table provenance");
        Function function = getFunctionAt(toAddr(ENTRY));
        if (function == null || !function.getName().equals("FUN_0011f7ef"))
            throw new IllegalStateException("unexpected dispatch owner");
        long before = function.getBody().getNumAddresses();
        AddressSet body = new AddressSet(toAddr(ENTRY), toAddr(END-1));
        boolean complete = function.getBody().equals(body);
        if (!complete && !function.getBody().equals(new AddressSet(toAddr(ENTRY), toAddr(ENTRY+9))))
            throw new IllegalStateException("unexpected previous dispatch body");
        long[] targets = new long[65];
        for (int i = 0; i < 64; ++i) {
            targets[i] = Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(TABLE+i*4)));
            if (targets[i] < ENTRY+10 || targets[i] >= END || (i > 0 && targets[i] <= targets[i-1]))
                throw new IllegalStateException("unexpected table target ordering");
        }
        if (targets[0] != ENTRY+10) throw new IllegalStateException("missing first case");
        targets[64] = END;
        List<String> rows = new ArrayList<>();
        rows.add("entry\tcase_index\tfrom\tend_exclusive\tinstructions\tcalls\tterminal\tbody_bytes_before\tbody_bytes_after\tresult");
        int transaction = currentProgram.startTransaction("reviewed complete division dispatch recovery");
        boolean commit = false;
        try {
            for (long offset = ENTRY; offset < END; ++offset) {
                Function owner = getFunctionContaining(toAddr(offset));
                if ((owner != null && owner != function) ||
                        currentProgram.getListing().getDefinedDataContaining(toAddr(offset)) != null)
                    throw new IllegalStateException("unreviewed code/data ownership at " + toAddr(offset));
            }
            int totalInstructions = 0, calls = 0, returns = 0, traps = 0;
            for (int i = 0; i < 64; ++i) {
                long cursor = targets[i], end = targets[i+1];
                int count = 0, caseCalls = 0;
                Instruction previous = null, last = null;
                while (cursor < end) {
                    Instruction instruction = getInstructionAt(toAddr(cursor));
                    if (instruction == null) {
                        if (getInstructionContaining(toAddr(cursor)) != null ||
                                !new DisassembleCommand(toAddr(cursor), body, false).applyTo(currentProgram, monitor))
                            throw new IllegalStateException("cannot decode case at " + toAddr(cursor));
                        instruction = getInstructionAt(toAddr(cursor));
                    }
                    if (instruction == null || cursor+instruction.getLength() > end)
                        throw new IllegalStateException("instruction crosses case boundary");
                    String mnemonic = instruction.getMnemonicString();
                    boolean terminal = cursor+instruction.getLength() == end;
                    if (terminal ? !Set.of("RET", "INT").contains(mnemonic) :
                            !Set.of("FDIV", "FDIVR", "FDIVP", "FDIVRP", "FSTP", "FLD", "FXCH", "CALL", "ADD").contains(mnemonic))
                        throw new IllegalStateException("unreviewed case flow at " + toAddr(cursor));
                    if (mnemonic.equals("CALL")) {
                        if (instruction.getFlows().length != 1 || instruction.getFlows()[0].getOffset() != HELPER)
                            throw new IllegalStateException("unreviewed case callee");
                        ++caseCalls; ++calls;
                    }
                    if (mnemonic.equals("ADD") &&
                            (instruction.getRegister(0) == null || !instruction.getRegister(0).getName().equals("ESP") ||
                             instruction.getScalar(1) == null || instruction.getScalar(1).getUnsignedValue() != 32 ||
                             cursor+instruction.getLength() >= end))
                        throw new IllegalStateException("unreviewed stack cleanup");
                    previous = last; last = instruction;
                    cursor += instruction.getLength(); ++count; ++totalInstructions;
                }
                if (last == null || previous == null || !previous.getMnemonicString().equals("ADD") ||
                        (last.getMnemonicString().equals("RET") && last.getNumOperands() != 0) ||
                        (last.getMnemonicString().equals("INT") &&
                         (last.getScalar(0) == null || last.getScalar(0).getUnsignedValue() != 6)))
                    throw new IllegalStateException("unreviewed case exit");
                if (last.getMnemonicString().equals("RET")) ++returns; else ++traps;
                rows.add(String.format("0x%08x\t%d\t0x%08x\t0x%08x\t%d\t%d\t%s\t%d\t%d\t%s",
                    ENTRY, i, targets[i], end, count, caseCalls, last.getMnemonicString(), before, END-ENTRY,
                    complete ? "already_recovered" : args[1].equals("apply") ? "recovered" : "proposed"));
            }
            if (totalInstructions != 374 || calls != 42 || returns != 48 || traps != 16)
                throw new IllegalStateException("unexpected complete dispatch shape");
            Instruction jump = getInstructionAt(toAddr(JUMP));
            if (jump == null || !jump.getFlowType().isComputed() || !jump.getFlowType().isJump())
                throw new IllegalStateException("missing computed dispatch jump");
            for (int i = 0; i < 64; ++i)
                currentProgram.getReferenceManager().addMemoryReference(toAddr(JUMP), toAddr(targets[i]),
                    RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
            function.setBody(body);
            if (!function.getBody().equals(body)) throw new IllegalStateException("incomplete recovered body");
            commit = args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction, commit);
        }
        var output = Paths.get(args[0]);
        Files.createDirectories(output.getParent());
        Files.writeString(output, String.join("\n", rows)+"\n");
        println("EF2000: audited all 64 division cases; " + args[1]);
    }
}
