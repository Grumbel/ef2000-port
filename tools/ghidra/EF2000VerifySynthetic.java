// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Data;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.symbol.Symbol;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Assert persisted annotations in the asset-free synthetic project. */
public class EF2000VerifySynthetic extends GhidraScript {
    @Override protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("expected output marker");
        Function function = getFunctionAt(toAddr(0));
        if (function == null || !function.getName().equals("fixture_recovered") ||
                !function.hasCustomVariableStorage() || function.getParameterCount() != 1 ||
                !function.getReturn().getVariableStorage().toString().contains("EAX"))
            throw new IllegalStateException("function name/storage did not survive reload");
        Function voidFunction = getFunctionAt(toAddr(0x10));
        if (voidFunction == null || !voidFunction.getName().equals("fixture_void") ||
                voidFunction.getParameterCount() != 0 ||
                !voidFunction.getReturnType().isEquivalent(VoidDataType.dataType))
            throw new IllegalStateException("void function signature did not survive reload");
        Symbol global = currentProgram.getSymbolTable().getGlobalSymbol("fixture_value", toAddr(0x20));
        Data data = getDataAt(toAddr(0x20));
        if (global == null || data == null || data.getLength() != 4 || !data.isDefined())
            throw new IllegalStateException("typed global did not survive reload");
        List<DataType> types = new ArrayList<>();
        currentProgram.getDataTypeManager().findDataTypes("fixture_record", types);
        if (types.isEmpty() || types.stream().noneMatch(t -> t.getLength() == 4))
            throw new IllegalStateException("CParser struct was not retained");
        Files.writeString(Paths.get(args[0]), "verified\n", StandardCharsets.UTF_8);
    }
}
