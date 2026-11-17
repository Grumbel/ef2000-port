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

/** Repair the reviewed drawing tail reached by fallthrough at 0x4c2c5. */
public class EF2000Recover4c120Tail extends GhidraScript {
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
           !hash(0x4c2c5,91).equals("7c8d74464c4df342605e44b711de9e371cff24daabd719ad527799b0b1cfc49c"))
            throw new IllegalStateException("unexpected language/image/tail provenance");
        Function function=getFunctionAt(toAddr(0x4c120));
        AddressSet old=new AddressSet(toAddr(0x4c120),toAddr(0x4c2c4));
        old.add(toAddr(0x4c320),toAddr(0x4c338));
        AddressSet body=new AddressSet(toAddr(0x4c120),toAddr(0x4c338));
        Instruction branch=getInstructionAt(toAddr(0x4c2c3));
        if(function==null || (!function.getBody().equals(old)&&!function.getBody().equals(body)) ||
           branch==null || !branch.getMnemonicString().equals("MOV") ||
           !toAddr(0x4c2c5).equals(branch.getFallThrough()))
            throw new IllegalStateException("unexpected previous body or tail branch");
        for(long offset=0x4c2c5;offset<=0x4c31f;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed tail ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed 4c120 tail hole");
        boolean commit=false;
        try {
            clearListing(toAddr(0x4c2c5),toAddr(0x4c31f));
            if(!disassemble(toAddr(0x4c2c5)))
                throw new IllegalStateException("tail disassembly failed");
            int[] lengths={5,4,3,3,4,5,3,4,4,5,1,3,4,2,3,4,3,2,2,2,5,4,4,2,1,2,2,5};
            String[] mnemonics={"CALL","MOV","SUB","MOVSX","MOV","ADD","MOVSX","MOV","MOV","ADD","PUSH","MOVSX","MOV","MOV","SUB","MOV","MOVSX","MOV","MOV","MOV","CALL","MOV","MOV","MOV","PUSH","MOV","MOV","CALL"};
            long offset=0x4c2c5;
            for(int index=0;index<lengths.length;++index) {
                Instruction insn=getInstructionAt(toAddr(offset));
                if(insn==null || insn.getLength()!=lengths[index] ||
                   !insn.getMnemonicString().equals(mnemonics[index]))
                    throw new IllegalStateException("unexpected recovered tail at "+toAddr(offset));
                offset+=lengths[index];
            }
            if(offset!=0x4c320) throw new IllegalStateException("incorrect tail extent");
            function.setBody(body);
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes_before\tbody_bytes_after\tresult\n"+
                "0x0004c120\t"+before+"\t"+body.getNumAddresses()+"\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n");
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: 4c120 tail boundary report: "+args[0]);
    }
}
