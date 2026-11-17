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

/** Repair the reviewed shading tail reached by fallthrough at 0xc6437. */
public class EF2000RecoverC6428Tail extends GhidraScript {
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
           !hash(0xc6437,6).equals("e17b5e5cf9e120a568592aa3836d738017f3e47283e57cb9ba319b1003c57d7f"))
            throw new IllegalStateException("unexpected language/image/tail provenance");
        Function function=getFunctionAt(toAddr(0xc6428));
        AddressSet old=new AddressSet(toAddr(0xc6428),toAddr(0xc6436));
        AddressSet body=new AddressSet(toAddr(0xc6428),toAddr(0xc643c));
        Instruction branch=getInstructionAt(toAddr(0xc6436));
        if(function==null || (!function.getBody().equals(old)&&!function.getBody().equals(body)) ||
           branch==null || !branch.getMnemonicString().equals("XLAT") ||
           !toAddr(0xc6437).equals(branch.getFallThrough()))
            throw new IllegalStateException("unexpected previous body or tail branch");
        for(long offset=0xc6437;offset<=0xc643c;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed tail ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed c6428 store/return tail");
        boolean commit=false;
        try {
            clearListing(toAddr(0xc6437),toAddr(0xc643c));
            if(!disassemble(toAddr(0xc6437)))
                throw new IllegalStateException("tail disassembly failed");
            int[] lengths={5,1};
            String[] mnemonics={"MOV","RET"};
            long offset=0xc6437;
            for(int index=0;index<lengths.length;++index) {
                Instruction insn=getInstructionAt(toAddr(offset));
                if(insn==null || insn.getLength()!=lengths[index] ||
                   !insn.getMnemonicString().equals(mnemonics[index]))
                    throw new IllegalStateException("unexpected recovered tail at "+toAddr(offset));
                offset+=lengths[index];
            }
            if(offset!=0xc643d) throw new IllegalStateException("incorrect tail extent");
            function.setBody(body);
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes_before\tbody_bytes_after\tresult\n"+
                "0x000c6428\t"+before+"\t"+body.getNumAddresses()+"\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n");
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: c6428 tail boundary report: "+args[0]);
    }
}
