// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.Address;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Apply reviewed symbol facts, never inferred decompiler signatures. */
public class EF2000ApplySymbols extends GhidraScript {
    private CParser parser;

    private List<String[]> rows(Path file, String header) throws Exception {
        List<String> lines = Files.readAllLines(file, StandardCharsets.UTF_8);
        if (lines.isEmpty() || !lines.get(0).equals(header))
            throw new IllegalArgumentException("unexpected TSV header: " + file);
        List<String[]> result = new ArrayList<>();
        int columns = header.split("\t", -1).length;
        for (int i = 1; i < lines.size(); ++i) {
            if (lines.get(i).isEmpty()) continue;
            String[] row = lines.get(i).split("\t", -1);
            if (row.length != columns) throw new IllegalArgumentException(file + ":" + (i + 1));
            result.add(row);
        }
        return result;
    }

    private Address addr(String field) {
        long value = Long.decode(field);
        if (value < 0 || value > 0xffffffffL ||
                currentProgram.getMemory().getBlock(toAddr(value)) == null)
            throw new IllegalArgumentException("address not mapped: " + field);
        return toAddr(value);
    }

    private DataType type(String spec) throws Exception {
        switch (spec.trim()) {
            case "uint8_t": return ByteDataType.dataType;
            case "int8_t": return SignedByteDataType.dataType;
            case "uint16_t": return WordDataType.dataType;
            case "int16_t": return SignedWordDataType.dataType;
            case "uint32_t": return DWordDataType.dataType;
            case "int32_t": return SignedDWordDataType.dataType;
            case "void": return VoidDataType.dataType;
            case "fword": return new ArrayDataType(ByteDataType.dataType, 6, 1);
            default:
                // CParser supplies the same struct/type namespace as parsed headers.
                DataType parsed = parser.parse(spec);
                if (parsed == null) throw new IllegalArgumentException("unknown type: " + spec);
                return parsed;
        }
    }

    private void parseHeaders(Path directory) throws Exception {
        parser = new CParser(currentProgram.getDataTypeManager());
        // Parse declarative headers in deterministic order. CParser does not
        // invoke a C preprocessor; the fixed-width aliases are predeclared.
        parser.parse("typedef unsigned char uint8_t; typedef signed char int8_t; " +
            "typedef unsigned short uint16_t; typedef short int16_t; " +
            "typedef unsigned int uint32_t; typedef int int32_t; " +
            "typedef uint32_t ef_addr32;");
        if (!Files.isDirectory(directory)) throw new IllegalArgumentException("missing headers");
        try (java.util.stream.Stream<Path> files = Files.walk(directory)) {
            for (Path header : (Iterable<Path>) files.filter(p -> p.toString().endsWith(".h"))
                    .sorted()::iterator) {
                String source = Files.readString(header);
                source = source.replaceAll("(?m)^\\s*#.*$", "");
                source = source.replace("EF_PACKED", "");
                // ef_globals.h contains extern declarations with overlaps;
                // they are applied from globals.tsv, not parsed as structs.
                if (header.getFileName().toString().equals("ef_globals.h") ||
                        header.getFileName().toString().equals("ef_types.h") ||
                        header.getFileName().toString().equals("ef_abi.h")) continue;
                if (!source.trim().isEmpty()) {
                    parser.parse(source);
                    // CParser's dictionary is transient unless its recovered
                    // composites are explicitly installed in the program DTM.
                    for (DataType dataType : parser.getComposites().values()) {
                        currentProgram.getDataTypeManager().addDataType(dataType,
                            DataTypeConflictHandler.DEFAULT_HANDLER);
                    }
                }
            }
        }
    }

    private Variable[] parameters(String inputs) throws Exception {
        if (inputs.isBlank()) return new Variable[0];
        List<Variable> parameters = new ArrayList<>();
        for (String part : inputs.split(",")) {
            String[] fields = part.trim().split(":", -1);
            if (fields.length != 2) throw new IllegalArgumentException("input: " + part);
            Register register = currentProgram.getLanguage().getRegister(fields[0]);
            if (register == null) throw new IllegalArgumentException("unknown register: " + fields[0]);
            parameters.add(new ParameterImpl("in_" + fields[0].toLowerCase(Locale.ROOT),
                type(fields[1]), new VariableStorage(currentProgram, register), currentProgram));
        }
        return parameters.toArray(new Variable[0]);
    }

    @Override protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 5) throw new IllegalArgumentException(
            "expected functions.tsv globals.tsv signatures.tsv include-directory success-marker");
        parseHeaders(Paths.get(args[3]));
        int names = 0, globals = 0, signatures = 0;
        for (String[] row : rows(Paths.get(args[0]),
                "addr\tsize\tname\tmodule\tstate\tkind\tsig_status\ttest\tnotes")) {
            if (row[2].isEmpty()) continue;
            Function function = getFunctionAt(addr(row[0]));
            if (function == null) throw new IllegalArgumentException("missing function " + row[0]);
            if (!function.getName().equals(row[2])) {
                function.setName(row[2], SourceType.USER_DEFINED);
                names++;
            }
        }
        for (String[] row : rows(Paths.get(args[1]),
                "addr\tsize\ttype\tname\tevidence\tnotes")) {
            Address address = addr(row[0]);
            Symbol symbol = currentProgram.getSymbolTable().getGlobalSymbol(row[3], address);
            if (symbol == null) {
                currentProgram.getSymbolTable().createLabel(address, row[3],
                    SourceType.USER_DEFINED);
                globals++;
            }
            // Do not overwrite overlapping declarations (e.g. _Envptr and
            // _Envseg). The label is still useful and remains reproducible.
            int size = Integer.parseInt(row[1]);
            if (size < 1 || size > 65536) throw new IllegalArgumentException("global size");
            DataType dataType;
            if (row[2].equals("char *")) dataType = new Pointer32DataType();
            else if (row[2].startsWith("struct "))
                dataType = new ArrayDataType(ByteDataType.dataType, size, 1);
            else dataType = type(row[2]);
            if (dataType.getLength() != size)
                throw new IllegalArgumentException("global size/type mismatch: " + row[3]);
            Data existing = getDataContaining(address);
            if (existing == null || !existing.isDefined()) {
                DataUtilities.createData(currentProgram, address, dataType, size, false,
                    DataUtilities.ClearDataMode.CLEAR_ALL_UNDEFINED_CONFLICT_DATA);
            }
        }
        for (String[] row : rows(Paths.get(args[2]),
                "addr\tname\tinputs\toutputs\tpreserved\tstack_bytes\tevidence")) {
            Function function = getFunctionAt(addr(row[0]));
            if (function == null || !function.getName().equals(row[1]))
                throw new IllegalArgumentException("signature has no matching function: " + row[0]);
            // Only confirmed signatures are placed in this table. Outputs
            // beyond one register and stack cleanup need step 3.5 ABI work.
            if (!row[5].equals("0")) throw new IllegalArgumentException("unsupported stack cleanup");
            Variable[] params = parameters(row[2]);
            function.updateFunction(null, null, FunctionUpdateType.CUSTOM_STORAGE,
                true, SourceType.USER_DEFINED, params);
            if (!row[3].isBlank()) {
                String[] output = row[3].split(":", -1);
                if (output.length != 2) throw new IllegalArgumentException("output: " + row[3]);
                Register register = currentProgram.getLanguage().getRegister(output[0]);
                if (register == null) throw new IllegalArgumentException("output register");
                function.setReturn(type(output[1]), new VariableStorage(currentProgram, register),
                    SourceType.USER_DEFINED);
            }
            signatures++;
        }
        println("EF2000 symbols: " + names + " names, " + globals +
            " global labels, " + signatures + " custom signatures applied");
        Files.writeString(Paths.get(args[4]), "applied\n", StandardCharsets.UTF_8);
    }
}
