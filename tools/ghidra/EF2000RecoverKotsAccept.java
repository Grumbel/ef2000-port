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

/** Repair the reviewed branch region hole reached by the conditional branch at 0x52a0. */
public class EF2000RecoverKotsAccept extends GhidraScript {
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
           !hash(0x5327,350).equals("f3e234459606af9c7dc7969c1c85173d6699e29cff539b5632f7fb099057a44c"))
            throw new IllegalStateException("unexpected language/image/branch region provenance");
        Function function=getFunctionAt(toAddr(0x522c));
        AddressSet old=new AddressSet(toAddr(0x522c),toAddr(0x5326));
        AddressSet body=new AddressSet(toAddr(0x522c),toAddr(0x5484));
        Instruction branch=getInstructionAt(toAddr(0x52a0));
        if(function==null || (!function.getBody().equals(old)&&!function.getBody().equals(body)) ||
           branch==null || !branch.getMnemonicString().equals("JNZ") ||
           branch.getFlows().length!=1 || !branch.getFlows()[0].equals(toAddr(0x5327)))
            throw new IllegalStateException("unexpected previous body or branch region branch");
        for(long offset=0x5327;offset<=0x5484;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed branch region ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed kotsAccept branches hole");
        boolean commit=false;
        try {
            clearListing(toAddr(0x5327),toAddr(0x5484));
            if(!disassemble(toAddr(0x5327)))
                throw new IllegalStateException("branch region disassembly failed");
            long offset=0x5327;
            int count=0;
            while(offset<0x5485) {
                Instruction insn=getInstructionAt(toAddr(offset));
                if(insn==null || insn.getLength()<1 || offset+insn.getLength()>0x5485)
                    throw new IllegalStateException("incomplete branch instruction at "+toAddr(offset));
                offset+=insn.getLength();
                ++count;
            }
            if(count!=104 || !getInstructionAt(toAddr(0x53d3)).getMnemonicString().equals("RET") ||
               !getInstructionAt(toAddr(0x5484)).getMnemonicString().equals("RET"))
                throw new IllegalStateException("unexpected recovered branch layout");
            function.setBody(body);
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes_before\tbody_bytes_after\tresult\n"+
                "0x0000522c\t"+before+"\t"+body.getNumAddresses()+"\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n");
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: kotsAccept branches boundary report: "+args[0]);
    }
}
