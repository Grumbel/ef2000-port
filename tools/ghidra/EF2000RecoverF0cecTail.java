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

/** Repair the reviewed soft-float tail reached by fallthrough at 0xf0e09. */
public class EF2000RecoverF0cecTail extends GhidraScript {
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
           !hash(0xf0e09,183).equals("be41d5347a8994a54d49e6d7d538d2615caff571529a339774e99377d5735b5d"))
            throw new IllegalStateException("unexpected language/image/tail provenance");
        Function function=getFunctionAt(toAddr(0xf0cec));
        AddressSet old=new AddressSet(toAddr(0xf0cec),toAddr(0xf0e08));
        AddressSet body=new AddressSet(toAddr(0xf0cec),toAddr(0xf0ebf));
        Instruction branch=getInstructionAt(toAddr(0xf0e08));
        if(function==null || (!function.getBody().equals(old)&&!function.getBody().equals(body)) ||
           branch==null || !branch.getMnemonicString().equals("PUSH") ||
           !toAddr(0xf0e09).equals(branch.getFallThrough()))
            throw new IllegalStateException("unexpected previous body or tail branch");
        for(long offset=0xf0e09;offset<=0xf0ebf;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed tail ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed f0cec tail hole");
        boolean commit=false;
        try {
            clearListing(toAddr(0xf0e09),toAddr(0xf0ebf));
            if(!disassemble(toAddr(0xf0e09)))
                throw new IllegalStateException("tail disassembly failed");
            long offset=0xf0e09;
            int count=0;
            while(offset<0xf0ec0) {
                Instruction insn=getInstructionAt(toAddr(offset));
                if(insn==null || insn.getLength()<=0 || offset+insn.getLength()>0xf0ec0)
                    throw new IllegalStateException("non-contiguous recovered tail at "+toAddr(offset));
                offset+=insn.getLength();
                ++count;
            }
            if(count!=66) throw new IllegalStateException("unexpected instruction count");
            if(offset!=0xf0ec0) throw new IllegalStateException("incorrect tail extent");
            function.setBody(body);
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes_before\tbody_bytes_after\tresult\n"+
                "0x000f0cec\t"+before+"\t"+body.getNumAddresses()+"\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n");
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: f0cec tail boundary report: "+args[0]);
    }
}
