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
        Function stackFunction = getFunctionAt(toAddr(0x40));
        if (stackFunction == null || !stackFunction.getName().equals("fixture_stack") ||
                !stackFunction.hasCustomVariableStorage() ||
                stackFunction.getParameterCount() != 2 ||
                !stackFunction.getParameter(0).getVariableStorage().hasStackStorage() ||
                stackFunction.getParameter(0).getVariableStorage().getStackOffset() != 4 ||
                !stackFunction.getParameter(1).getVariableStorage().hasStackStorage() ||
                stackFunction.getParameter(1).getVariableStorage().getStackOffset() != 8 ||
                stackFunction.getStackPurgeSize() != 8 ||
                !stackFunction.getReturn().getVariableStorage().toString().contains("EAX"))
            throw new IllegalStateException("stack inputs / ret 8 did not survive reload");
        Symbol global = currentProgram.getSymbolTable().getGlobalSymbol("fixture_value", toAddr(0x20));
        Data data = getDataAt(toAddr(0x20));
        if (global == null || data == null || data.getLength() != 4 || !data.isDefined())
            throw new IllegalStateException("typed global did not survive reload");
        List<DataType> types = new ArrayList<>();
        currentProgram.getDataTypeManager().findDataTypes("fixture_record", types);
        if (types.isEmpty() || types.stream().noneMatch(t -> t.getLength() == 4))
            throw new IllegalStateException("CParser struct was not retained");
        types.clear();
        currentProgram.getDataTypeManager().findDataTypes("fixture_packed", types);
        if (types.stream().noneMatch(t -> t instanceof ghidra.program.model.data.Structure &&
                t.getLength() == 9 &&
                ((ghidra.program.model.data.Structure) t).getComponent(1).getOffset() == 5))
            throw new IllegalStateException("packed struct layout did not survive reload");
        Files.writeString(Paths.get(args[0]), "verified\n", StandardCharsets.UTF_8);
    }
}
