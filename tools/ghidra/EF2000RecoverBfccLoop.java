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

/** Repair the reviewed loop hole reached by the conditional branch at 0xc026. */
public class EF2000RecoverBfccLoop extends GhidraScript {
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
           !hash(0xbfff,28).equals("d2248ce77069d16848d290e34f1ccb06c4accf6f7a0b65dd0ae6313f38253d90"))
            throw new IllegalStateException("unexpected language/image/loop provenance");
        Function function=getFunctionAt(toAddr(0xbfcc));
        AddressSet old=new AddressSet(toAddr(0xbfcc),toAddr(0xbffe));
        old.add(toAddr(0xc01b),toAddr(0xc590));
        AddressSet body=new AddressSet(toAddr(0xbfcc),toAddr(0xc590));
        Instruction branch=getInstructionAt(toAddr(0xc026));
        if(function==null || (!function.getBody().equals(old)&&!function.getBody().equals(body)) ||
           branch==null || !branch.getMnemonicString().equals("JL") ||
           branch.getFlows().length!=1 || !branch.getFlows()[0].equals(toAddr(0xbfff)))
            throw new IllegalStateException("unexpected previous body or loop branch");
        for(long offset=0xbfff;offset<=0xc01a;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed loop ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed bfcc loop hole");
        boolean commit=false;
        try {
            clearListing(toAddr(0xbfff),toAddr(0xc01a));
            if(!disassemble(toAddr(0xbfff)))
                throw new IllegalStateException("loop disassembly failed");
            int[] lengths={6,2,2,3,2,3,3,3,3,1};
            String[] mnemonics={"MOV","CMP","JZ","CMP","JZ","MOVSX","IMUL","MOV","MOV","INC"};
            long offset=0xbfff;
            for(int index=0;index<lengths.length;++index) {
                Instruction insn=getInstructionAt(toAddr(offset));
                if(insn==null || insn.getLength()!=lengths[index] ||
                   !insn.getMnemonicString().equals(mnemonics[index]))
                    throw new IllegalStateException("unexpected recovered loop at "+toAddr(offset));
                offset+=lengths[index];
            }
            if(offset!=0xc01b) throw new IllegalStateException("incorrect loop extent");
            function.setBody(body);
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes_before\tbody_bytes_after\tresult\n"+
                "0x0000bfcc\t"+before+"\t"+body.getNumAddresses()+"\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n");
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: bfcc loop boundary report: "+args[0]);
    }
}
