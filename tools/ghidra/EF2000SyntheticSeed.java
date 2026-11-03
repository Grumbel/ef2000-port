// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

/** Asset-free fixture: entries at zero, 0x10, 0x40 and 0x80; data at 0x20. */
public class EF2000SyntheticSeed extends GhidraScript {
    @Override protected void run() throws Exception {
        if (getInstructionAt(toAddr(0)) == null && !disassemble(toAddr(0)))
            throw new IllegalStateException("cannot disassemble fixture");
        Function function = getFunctionAt(toAddr(0));
        if (function == null) function = createFunction(toAddr(0), "fixture_original");
        if (function == null) throw new IllegalStateException("cannot create fixture function");
        if (getInstructionAt(toAddr(0x10)) == null && !disassemble(toAddr(0x10)))
            throw new IllegalStateException("cannot disassemble void fixture");
        Function voidFunction = getFunctionAt(toAddr(0x10));
        if (voidFunction == null) voidFunction = createFunction(toAddr(0x10), "fixture_void_old");
        if (voidFunction == null) throw new IllegalStateException("cannot create void function");
        if (getInstructionAt(toAddr(0x40)) == null && !disassemble(toAddr(0x40)))
            throw new IllegalStateException("cannot disassemble stack fixture");
        Function stackFunction = getFunctionAt(toAddr(0x40));
        if (stackFunction == null)
            stackFunction = createFunction(toAddr(0x40), "fixture_stack_old");
        if (stackFunction == null) throw new IllegalStateException("cannot create stack function");
        if (getInstructionAt(toAddr(0x80)) == null && !disassemble(toAddr(0x80)))
            throw new IllegalStateException("cannot disassemble x87 fixture");
        Function x87Function = getFunctionAt(toAddr(0x80));
        if (x87Function == null)
            x87Function = createFunction(toAddr(0x80), "fixture_x87_old");
        if (x87Function == null) throw new IllegalStateException("cannot create x87 function");
    }
}
