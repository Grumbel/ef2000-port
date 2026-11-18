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
import ghidra.program.model.lang.Register;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.List;
import java.util.Set;

/** Reviewed callback bodies and entry prefixes exposed by checked indirect dispatch. */
public class EF2000RecoverObservedLeaves extends GhidraScript {
    private static final int IMAGE_SIZE = 0x195fb0;
    // entry, exclusive end, corroborating instruction, previous body bytes.
    // Both callbacks follow RET and precede independent caller prologues.
    // XLAT's missing flow annotation had cut the third body before its store/RET.
    // The fourth is the RET-only target of a reviewed indexed dispatch table.
    private static final long[][] RECIPES = {
        {0xa432f, 0xa4342, 0xa4352, 0},
        {0xe300f, 0xe3022, 0xe3026, 0},
        {0xc64d0, 0xc64ea, 0, 20},
        {0xc5e65, 0xc5e66, 0xc6cdf, 0},
        {0x1c53c, 0x1c545, 0x1c902, 0},
        {0xdf244, 0xdf248, 0xdf252, 0},
        {0xd83b8, 0xd849a, 0xd916f, 0},
        {0xd849c, 0xd8528, 0xd917b, 0},
        {0xd8168, 0xd8170, 0xd90b3, 0},
        {0xfa804, 0xfa815, 0xfa856, 0},
        {0xd8220, 0xd8358, 0xd90b8, 0},
        {0x122a0, 0x12318, 0, 0},
        {0x12f18, 0x12f9c, 0x12188, 0},
        {0x13a50, 0x13abc, 0x12dfd, 0},
        {0x1303c, 0x1309f, 0x12188, 0},
        {0x13510, 0x135c9, 0x12dfd, 0},
        {0x1f910, 0x1f95d, 0, 0},
        {0x13d8c, 0x13dd1, 0x12dfd, 0},
        {0x13e14, 0x13eb4, 0x12dfd, 0},
    };
    private static final String[] NAMES = {
        "format_buffer_putc_a432f", "format_buffer_putc_e300f", "shade_lookup_store_c64d0",
        "dispatch_return_c5e65", "dispatch_clear_latch_1c53c", "scan_stream_unget_df244",
        "position_callback_d83b8", "position_callback_d849c", "position_callback_prefix_d8168",
        "format_stream_putc_fa804", "position_callback_d8220",
        "dispatch_group_helper_122a0", "dispatch_group_12f18", "dispatch_group_13a50",
        "dispatch_group_1303c", "dispatch_group_13510", "dispatch_group_helper_1f910", "dispatch_group_13d8c", "dispatch_group_13e14"
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
                boolean positionCallback = entry == 0xd83b8 || entry == 0xd849c;
                boolean positionBranch = entry == 0xd8220;
                long groupSlot = switch ((int)entry) {
                    case 0x12f18 -> 0x121fea;
                    case 0x1303c -> 0x122007;
                    case 0x13510 -> 0x12222e;
                    case 0x13a50 -> 0x122333;
                    case 0x13d8c -> 0x1224e6;
                    case 0x13e14 -> 0x1223fe;
                    default -> 0;
                };
                Set<Long> directCallees = switch ((int)entry) {
                    case 0xd8220 -> Set.of(0xd8170L);
                    case 0x122a0 -> Set.of(0x11e7cL);
                    case 0x12f18, 0x13a50, 0x13e14 -> Set.of(0x122a0L);
                    case 0x1303c, 0x13510 -> Set.of(0x12d90L, 0x11d20L);
                    case 0x1f910 -> Set.of(0x22e98L, 0x1f878L);
                    case 0x13d8c -> Set.of(0x122a0L, 0x1f910L);
                    default -> Set.of();
                };
                boolean formatStream = entry == 0xfa804;
                boolean entryPrefix = entry == 0xd8168;
                Function function = getFunctionAt(toAddr(entry));
                long before = function == null ? 0 : function.getBody().getNumAddresses();
                if (before != recipe[3] && before != end-entry) {
                    throw new IllegalStateException("unexpected previous body at " + toAddr(entry));
                }
                if (entry == 0xc5e65) {
                    Instruction jump = getInstructionAt(toAddr(literal));
                    Function caller = getFunctionContaining(toAddr(literal));
                    boolean table = false, base = false;
                    if (jump != null && jump.getMnemonicString().equals("JMP")) {
                        for (Object object : jump.getOpObjects(0)) {
                            if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == 0x15b270) table = true;
                            if (object instanceof Address && ((Address)object).getOffset() == 0x15b270) table = true;
                            if (object instanceof Register && ((Register)object).getName().equals("EAX")) base = true;
                        }
                    }
                    if (!table || !base || caller == null || caller.getEntryPoint().getOffset() != 0xc6b80 ||
                            Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(0x15b270))) != entry) {
                        throw new IllegalStateException("missing observed dispatch-table corroboration");
                    }
                    Address trapAddress = toAddr(entry-1);
                    if (getInstructionAt(trapAddress) == null) {
                        if (currentProgram.getListing().getDefinedDataContaining(trapAddress) != null ||
                                getInstructionContaining(trapAddress) != null ||
                                !new DisassembleCommand(trapAddress, new AddressSet(trapAddress, trapAddress), false)
                                    .applyTo(currentProgram, monitor)) {
                            throw new IllegalStateException("cannot audit preceding trap boundary");
                        }
                    }
                    Instruction trap = getInstructionAt(trapAddress);
                    if (trap == null || !trap.getMnemonicString().equals("INT3") || trap.getLength() != 1) {
                        throw new IllegalStateException("unexpected preceding trap boundary");
                    }
                } else if (entry == 0x1c53c) {
                    Instruction call = getInstructionAt(toAddr(literal));
                    Function caller = getFunctionContaining(toAddr(literal));
                    boolean table = false, indexRegister = false;
                    if (call != null && call.getMnemonicString().equals("CALL") && call.getFlowType().isComputed()) {
                        for (Object object : call.getOpObjects(0)) {
                            if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == 0x122d34) table = true;
                            if (object instanceof Address && ((Address)object).getOffset() == 0x122d34) table = true;
                            if (object instanceof Register && ((Register)object).getName().equals("EAX")) indexRegister = true;
                        }
                    }
                    if (!table || !indexRegister || caller == null || caller.getEntryPoint().getOffset() != 0x1c780 ||
                            Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(0x122d34))) != entry) {
                        throw new IllegalStateException("missing observed callback-table corroboration");
                    }
                } else if (groupSlot != 0) {
                    Instruction call = getInstructionAt(toAddr(literal));
                    Function caller = getFunctionContaining(toAddr(literal));
                    long callerEntry = literal == 0x12188 ? 0x12160 : 0x12d90;
                    String indexRegister = literal == 0x12188 ? "EAX" : "ECX";
                    boolean base = false, register = false;
                    if (call != null && call.getFlowType().isCall() && call.getFlowType().isComputed()) {
                        for (Object object : call.getOpObjects(0)) {
                            if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == 0x121fcd) base = true;
                            if (object instanceof Address && ((Address)object).getOffset() == 0x121fcd) base = true;
                            if (object instanceof Register && ((Register)object).getName().equals(indexRegister)) register = true;
                        }
                    }
                    if (!base || !register || caller == null || caller.getEntryPoint().getOffset() != callerEntry ||
                            Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(groupSlot))) != entry) {
                        throw new IllegalStateException("missing group callback table corroboration");
                    }
                } else if (literal != 0) {
                    Instruction reference = getInstructionAt(toAddr(literal));
                    Function caller = getFunctionContaining(toAddr(literal));
                    String register = entry == 0xdf244 ? "ESI" :
                        entry == 0xd83b8 || entryPrefix ? "EBX" : entry == 0xd849c ? "EDX" : "ECX";
                    long callerEntry = entry == 0xdf244 ? 0xdf248 : positionCallback || entryPrefix || positionBranch ? 0xd8fd8 :
                        formatStream ? 0xfa815 : 0;
                    boolean matches = false;
                    if (reference != null && reference.getMnemonicString().equals("MOV") &&
                            reference.getRegister(0) != null && reference.getRegister(0).getName().equals(register) &&
                            caller != null && (callerEntry == 0 || caller.getEntryPoint().getOffset() == callerEntry)) {
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
                if (entryPrefix && (getFunctionAt(toAddr(end)) == null ||
                        getInstructionAt(toAddr(end)) == null)) {
                    throw new IllegalStateException("missing prefix fallthrough destination");
                }
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
                    if (entryPrefix) {
                        if (!mnemonic.equals(last ? "LEA" : "MOV") ||
                                !toAddr(cursor + instruction.getLength()).equals(instruction.getFallThrough())) {
                            throw new IllegalStateException("unexpected entry prefix flow at " + toAddr(cursor));
                        }
                    } else if (!directCallees.isEmpty()) {
                        var flow = instruction.getFlowType();
                        Address[] targets = instruction.getFlows();
                        boolean returns = mnemonic.equals("RET");
                        boolean hasFallthrough = !returns && (!flow.isJump() || flow.isConditional());
                        Address fallthrough = hasFallthrough ? toAddr(cursor + instruction.getLength()) : null;
                        if ((last && !returns) || flow.isComputed() || (flow.isTerminal() && !returns) ||
                                !java.util.Objects.equals(fallthrough, instruction.getFallThrough()) ||
                                targets.length != (flow.isCall() || flow.isJump() ? 1 : 0)) {
                            throw new IllegalStateException("unexpected callback flow at " + toAddr(cursor));
                        }
                        for (Address target : targets) {
                            if (flow.isCall() ? !directCallees.contains(target.getOffset()) || getFunctionAt(target) == null :
                                    !flow.isJump() || !body.contains(target)) {
                                throw new IllegalStateException("unexpected callback target at " + toAddr(cursor));
                            }
                        }
                    } else if (formatStream && cursor == 0xfa80b) {
                        Address target = toAddr(0x10f43b);
                        Address[] flows = instruction.getFlows();
                        if (!mnemonic.equals("CALL") || instruction.getFlowType().isComputed() ||
                                flows.length != 1 || !target.equals(flows[0]) ||
                                getFunctionAt(target) == null ||
                                !toAddr(0xfa810).equals(instruction.getFallThrough())) {
                            throw new IllegalStateException("unexpected formatter callback call");
                        }
                    } else if (last ? !mnemonic.equals("RET") :
                            !Set.of("PUSH", "POP", "MOV", "LEA", "INC", "LODS", "LODSW", "ADD", "XLAT", "XLATB").contains(mnemonic) &&
                            !(entry == 0x1c53c && cursor == entry && mnemonic.equals("XOR")) &&
                            !(entry == 0xdf244 && cursor == entry && mnemonic.equals("DEC")) &&
                            !(positionCallback && Set.of("SUB", "XOR", "SAR", "IMUL", "IDIV").contains(mnemonic))) {
                        throw new IllegalStateException("unexpected leaf control flow at " + toAddr(cursor));
                    }
                    cursor += instruction.getLength();
                }
                if (!directCallees.isEmpty()) {
                    var instructions = currentProgram.getListing().getInstructions(body, true);
                    while (instructions.hasNext()) {
                        for (Address target : instructions.next().getFlows()) {
                            if (getInstructionAt(target) == null) {
                                throw new IllegalStateException("callback target crosses an instruction: " + target);
                            }
                        }
                    }
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
        println("EF2000: audited " + RECIPES.length + " observed leaf boundaries; " + args[1]);
    }
}
