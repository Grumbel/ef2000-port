// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.ByteDataType;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HexFormat;

/** Recover the pinned CALL/POP/RET table accessor, without treating its table as code. */
public class EF2000RecoverDecimalTable extends GhidraScript {
    private static final long ENTRY=0x10e35f, TABLE=0x10e364, HELPER=0x10e414;
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
           !hash(ENTRY,0xb7).equals("0520afa989bb53d7505d151e28d61e9164d211873845b08c88cfba81fded1769"))
            throw new IllegalStateException("unexpected language/image/accessor provenance");
        Function function=getFunctionAt(toAddr(ENTRY));
        Instruction call=getInstructionAt(toAddr(ENTRY));
        Instruction pop=getInstructionAt(toAddr(HELPER));
        Instruction ret=getInstructionAt(toAddr(HELPER+1));
        if(function==null || call==null || pop==null || ret==null ||
           !call.getMnemonicString().equals("CALL") || call.getLength()!=5 ||
           call.getFlows().length!=1 || !call.getFlows()[0].equals(toAddr(HELPER)) ||
           !pop.toString().equals("POP EDI") || !ret.getMnemonicString().equals("RET"))
            throw new IllegalStateException("unexpected accessor instruction layout");
        AddressSet body=new AddressSet(toAddr(ENTRY),toAddr(TABLE-1));
        AddressSet old=new AddressSet(toAddr(ENTRY),toAddr(0x10e405));
        if(!function.getBody().equals(old)&&!function.getBody().equals(body))
            throw new IllegalStateException("unexpected previous accessor body");
        for(long offset=TABLE;offset<HELPER;++offset) {
            Function owner=getFunctionContaining(toAddr(offset));
            if(owner!=null&&owner!=function)
                throw new IllegalStateException("unreviewed table ownership at "+toAddr(offset));
        }
        long before=function.getBody().getNumAddresses();
        int transaction=currentProgram.startTransaction("reviewed decimal table accessor");
        boolean commit=false;
        try {
            function.setBody(body);
            // This call never returns to its own continuation: the callee pops
            // that address into EDI, then returns through the outer stack frame.
            call.setFlowOverride(FlowOverride.CALL_RETURN);
            clearListing(toAddr(TABLE),toAddr(HELPER-1));
            createData(toAddr(TABLE),new ArrayDataType(ByteDataType.dataType,(int)(HELPER-TABLE),1));
            if(function.getBody().getNumAddresses()!=5 ||
               getInstructionAt(toAddr(TABLE))!=null ||
               getDataAt(toAddr(TABLE)).getLength()!=HELPER-TABLE ||
               getInstructionAt(toAddr(HELPER))==null)
                throw new IllegalStateException("incomplete accessor recovery");
            String report="entry\tbody_bytes_before\tbody_bytes_after\ttable_start\ttable_bytes\thelper\tresult\n"+
                "0x0010e35f\t"+before+"\t5\t0x0010e364\t176\t0x0010e414\t"+
                (args[1].equals("apply")?"recovered":"proposed")+"\n";
            Files.writeString(Path.of(args[0]),report);
            commit=args[1].equals("apply");
        } finally {
            currentProgram.endTransaction(transaction,commit);
        }
        println("EF2000: decimal accessor boundary report: "+args[0]);
    }
}
