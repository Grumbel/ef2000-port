// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;

/** Repair the reviewed patched-offset loop tail reached by fallthrough at 0xc8c55. */
public class EF2000RecoverC8c34Tail extends GhidraScript {
    private String hash(long start, int length) throws Exception {
        byte[] bytes=new byte[length];
        if(currentProgram.getMemory().getBytes(toAddr(start),bytes)!=length)
            throw new IllegalStateException("short image read");
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    @Override
    protected void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length!=2 || (!args[1].equals("report")&&!args[1].equals("apply")))
            throw new IllegalArgumentException("expected output.tsv, report|apply");
        if(!currentProgram.getLanguageID().toString().equals("x86:LE:32:default") ||
           !hash(0,0x195fb0).equals("3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb") ||
           !hash(0xc8c55,32).equals("2c2539fb68ed4859d43fc6d73af4cede321749f6dab2f967ee628a7b0c6d8b80"))
            throw new IllegalStateException("unexpected language/image/tail provenance");
        Function function=getFunctionAt(toAddr(0xc8c34));
        AddressSet old=new AddressSet(toAddr(0xc8c34),toAddr(0xc8c54));
        old.add(toAddr(0xc454a),toAddr(0xc4564));
        AddressSet body=new AddressSet(old);
        body.add(toAddr(0xc8c55),toAddr(0xc8c74));
        Instruction branch=getInstructionAt(toAddr(0xc8c4f));
        if(function==null || (!function.getBody().equals(old)&&!function.getBody().equals(body)) ||
           branch==null || !branch.getMnemonicString().equals("JO") ||
           !toAddr(0xc8c55).equals(branch.getFallThrough()))
            throw new IllegalStateException("unexpected previous body or tail branch");
        for(long offset=0xc8c55;offset<=0xc8c74;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed tail ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed c8c34 patched-offset loop tail");
        boolean commit=false;
        try {
            clearListing(toAddr(0xc8c55),toAddr(0xc8c74));
            if(!disassemble(toAddr(0xc8c55)))
                throw new IllegalStateException("tail disassembly failed");
            int[] lengths={5,6,2,3,3,6,3,1,2,1};
            String[] mnemonics={"ADD","JO","MOV","MOV","MOV","MOV","ADD","DEC","JNZ","RET"};
            long offset=0xc8c55;
            for(int index=0;index<lengths.length;++index) {
                Instruction insn=getInstructionAt(toAddr(offset));
                if(insn==null || insn.getLength()!=lengths[index] ||
                   !insn.getMnemonicString().equals(mnemonics[index]))
                    throw new IllegalStateException("unexpected recovered tail at "+toAddr(offset));
                offset+=lengths[index];
            }
            if(offset!=0xc8c75) throw new IllegalStateException("incorrect tail extent");
            function.setBody(body);
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes_before\tbody_bytes_after\tresult\n"+
                "0x000c8c34\t"+before+"\t"+body.getNumAddresses()+"\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n");
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: c8c34 tail boundary report: "+args[0]);
    }
}
