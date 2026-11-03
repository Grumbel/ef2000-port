// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

/** Asset-free fixture: entry at zero, global data at 0x20. */
public class EF2000SyntheticSeed extends GhidraScript {
    @Override protected void run() throws Exception {
        if (getInstructionAt(toAddr(0)) == null && !disassemble(toAddr(0)))
            throw new IllegalStateException("cannot disassemble fixture");
        Function function = getFunctionAt(toAddr(0));
        if (function == null) function = createFunction(toAddr(0), "fixture_original");
        if (function == null) throw new IllegalStateException("cannot create fixture function");
    }
}
