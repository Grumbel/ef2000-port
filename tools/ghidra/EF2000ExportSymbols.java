// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.DataType;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.lang.Register;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Export GUI-edited names as review candidates, leaving tracked facts intact. */
public class EF2000ExportSymbols extends GhidraScript {
    private List<String[]> read(Path path, String header) throws Exception {
        List<String> lines = Files.readAllLines(path, StandardCharsets.UTF_8);
        if (lines.isEmpty() || !lines.get(0).equals(header))
            throw new IllegalArgumentException("bad source TSV: " + path);
        List<String[]> rows = new ArrayList<>();
        for (int i = 1; i < lines.size(); ++i) {
            String[] row = lines.get(i).split("\t", -1);
            if (row.length != header.split("\t", -1).length)
                throw new IllegalArgumentException("bad row " + (i + 1));
            rows.add(row);
        }
        return rows;
    }

    private void write(Path path, String header, List<String[]> rows) throws Exception {
        List<String> lines = new ArrayList<>();
        lines.add(header);
        for (String[] row : rows) lines.add(String.join("\t", row));
        Files.write(path, lines, StandardCharsets.UTF_8);
    }

    private String storage(Variable variable) {
        Varnode[] nodes = variable.getVariableStorage().getVarnodes();
        if (nodes.length != 1 || !nodes[0].isRegister()) return "";
        Register register = currentProgram.getRegister(nodes[0].getAddress(), nodes[0].getSize());
        if (register == null) return "";
        return register.getName() + ":" + variable.getDataType().getName();
    }

    @Override protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 4) throw new IllegalArgumentException(
            "expected functions.tsv globals.tsv signatures.tsv output-dir");
        Path output = Paths.get(args[3]);
        Files.createDirectories(output);
        String fh = "addr\tsize\tname\tmodule\tstate\tkind\tsig_status\ttest\tnotes";
        String gh = "addr\tsize\ttype\tname\tevidence\tnotes";
        String sh = "addr\tname\tinputs\toutputs\tpreserved\tstack_bytes\tevidence";
        List<String[]> functions = read(Paths.get(args[0]), fh);
        List<String[]> globals = read(Paths.get(args[1]), gh);
        List<String[]> signatures = read(Paths.get(args[2]), sh);
        Set<Long> signedFunctions = new HashSet<>();
        for (String[] row : signatures) signedFunctions.add(Long.decode(row[0]));
        int renamed = 0;
        for (String[] row : functions) {
            long offset = Long.decode(row[0]);
            Function function = getFunctionAt(toAddr(offset));
            if (function == null) throw new IllegalArgumentException("missing " + row[0]);
            String name = function.getName();
            if (function.getSymbol().getSource() == SourceType.USER_DEFINED &&
                    !name.equals(row[2])) {
                row[2] = name;
                if (row[4].equals("unknown")) row[4] = "named";
                row[8] += (row[8].isEmpty() ? "" : "; ") + "GUI name candidate; review evidence";
                renamed++;
            }
        }
        Set<Long> knownGlobalOffsets = new HashSet<>();
        for (String[] row : globals) {
            knownGlobalOffsets.add(Long.decode(row[0]));
            // Preserve evidence, addresses and types exactly; GUI-created
            // globals without known sizes cannot safely be inferred here.
            if (getDataContaining(toAddr(Long.decode(row[0]))) == null)
                println("EF2000: untyped global candidate " + row[0]);
        }
        int newGlobals = 0;
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getSource() != SourceType.USER_DEFINED ||
                    symbol.getSymbolType() != SymbolType.LABEL ||
                    !symbol.getAddress().isMemoryAddress()) continue;
            long offset = symbol.getAddress().getOffset();
            if (knownGlobalOffsets.contains(offset) ||
                    getFunctionContaining(symbol.getAddress()) != null) continue;
            Data data = getDataAt(symbol.getAddress());
            if (data == null || !data.isDefined()) continue;
            DataType type = data.getDataType();
            globals.add(new String[] {String.format("0x%08x", offset),
                String.valueOf(data.getLength()), type.getName(), symbol.getName(),
                "", "GUI data candidate; review type and evidence"});
            knownGlobalOffsets.add(offset);
            newGlobals++;
        }
        globals.sort(Comparator.comparingLong(row -> Long.decode(row[0])));
        int signatureCandidates = 0;
        for (String[] row : functions) {
            long offset = Long.decode(row[0]);
            Function function = getFunctionAt(toAddr(offset));
            if (signedFunctions.contains(offset) || !function.hasCustomVariableStorage() ||
                    function.getSignatureSource() != SourceType.USER_DEFINED) continue;
            List<String> inputs = new ArrayList<>();
            boolean supported = true;
            for (Parameter param : function.getParameters()) {
                String reg = storage(param);
                if (reg.isEmpty()) { supported = false; break; }
                inputs.add(reg);
            }
            String result = storage(function.getReturn());
            if (!supported) continue;
            signatures.add(new String[] {row[0], function.getName(),
                String.join(",", inputs), result, "", "",
                "GUI custom storage candidate; verify calling convention and callers"});
            signatureCandidates++;
        }
        signatures.sort(Comparator.comparingLong(row -> Long.decode(row[0])));
        // Do not promote a GUI signature to 'confirmed' without corroborating
        // disassembly and callers. The existing records are carried forward.
        write(output.resolve("functions.tsv"), fh, functions);
        write(output.resolve("globals.tsv"), gh, globals);
        write(output.resolve("signatures.tsv"), sh, signatures);
        println("EF2000: exported " + renamed + " renamed functions, " +
            newGlobals + " new globals and " + signatureCandidates +
            " ABI candidates for review to " + output);
    }
}
