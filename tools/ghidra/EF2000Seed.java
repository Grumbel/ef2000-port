// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;

/** Annotate the pinned, selector-relative EF2000 payload before analysis. */
public class EF2000Seed extends GhidraScript {
    private static final long IMAGE_END = 0x195fb0L;
    private static final long BSS_END = 0x4de2f0L;

    @Override
    protected void run() throws Exception {
        if (!currentProgram.getLanguageID().toString().equals("x86:LE:32:default")) {
            throw new IllegalStateException("EF2000 requires x86:LE:32:default");
        }
        Memory memory = currentProgram.getMemory();
        if (memory.getBlock(toAddr(0)) == null ||
                memory.getBlock(toAddr(IMAGE_END - 1)) == null ||
                memory.getBlock(toAddr(IMAGE_END)) != null) {
            throw new IllegalStateException("not the 0-based EF2000 X-32 image");
        }
        // These offsets are the runtime's selector-relative addresses, not DOS
        // file offsets. Ghidra's raw loader does not know about the zeroed BSS.
        MemoryBlock bss = memory.createUninitializedBlock(
            "X32_BSS", toAddr(IMAGE_END), BSS_END - IMAGE_END, false);
        bss.setRead(true);
        bss.setWrite(true);
        bss.setExecute(false);

        // Strong identifications from the Watcom startup match and native
        // copy-entry profiling; the main entry is explicitly only a candidate.
        seedFunction(0x86, "x386_startup");
        seedFunction(0x9bd70, "watcom_init_routines");
        seedFunction(0x9bd20, "watcom_cmain");
        seedFunction(0x8b274, "candidate_game_main");
        seedFunction(0xe9a24, "original_memcpy");

        seedData(0x132a28, "x32_stack_size");
        seedData(0x132a2c, "dynend");
        seedData(0x132a30, "curbrk");
        seedData(0x132a34, "lp_cmd_line");
        seedData(0x132a38, "lp_program_name");
        seedData(0x132a3c, "psp_selector");
        seedData(0x132a61, "envptr");
        println("EF2000: seeded X-32 entry points, labels and uninitialized BSS");
    }

    private void seedFunction(long offset, String name) throws Exception {
        Address address = toAddr(offset);
        addEntryPoint(address);
        if (getInstructionAt(address) == null && !disassemble(address)) {
            throw new IllegalStateException("cannot disassemble " + name + " at " + address);
        }
        Function function = getFunctionAt(address);
        if (function == null) {
            function = createFunction(address, name);
        }
        if (function == null) {
            throw new IllegalStateException("cannot create " + name + " at " + address);
        }
        function.setName(name, SourceType.USER_DEFINED);
    }

    private void seedData(long offset, String name) throws Exception {
        createLabel(toAddr(offset), name, true);
    }
}
