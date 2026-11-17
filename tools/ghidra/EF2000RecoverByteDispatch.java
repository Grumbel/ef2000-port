// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.HexFormat;
import java.util.Set;

/** Recover one pinned byte dispatcher without changing the preceding error body. */
public class EF2000RecoverByteDispatch extends GhidraScript {
    private static final long ENTRY=0x11dab0, END=0x11e31f;
    private static final long TABLE=0x11d6b0, JUMP=0x11db3e;
    private String hash(long start, int length) throws Exception {
        byte[] bytes=new byte[length];
        if(currentProgram.getMemory().getBytes(toAddr(start),bytes)!=length)
            throw new IllegalStateException("short image read");
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }
    @Override
    protected void run() throws Exception {
        String[] args=getScriptArgs();
        if(args.length!=2||(!args[1].equals("report")&&!args[1].equals("apply")))
            throw new IllegalArgumentException("expected output.tsv, report|apply");
        if(!currentProgram.getLanguageID().toString().equals("x86:LE:32:default")||
           !hash(0,0x195fb0).equals("3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb")||
           !hash(ENTRY,(int)(END-ENTRY)).equals("b54064922534caccc934c1a2ffb19eac5779e761c99ed741f424705e04fbceaf")||
           !hash(TABLE,1024).equals("6a19e4d300743c8189ac8af7ceb4a2901e10fa27c92952bfa907322dc90a0f2f"))
            throw new IllegalStateException("unexpected language/image/body/table provenance");
        AddressSet body=new AddressSet(toAddr(ENTRY),toAddr(END-1));
        Function function=getFunctionAt(toAddr(ENTRY));
        if(function!=null&&(!function.getName().equals("draw_byte_mask_11dab0")||!function.getBody().equals(body)))
            throw new IllegalStateException("unexpected existing function");
        for(long a=ENTRY;a<END;++a) {
            Function owner=getFunctionContaining(toAddr(a));
            if((owner!=null&&owner!=function)||getDataContaining(toAddr(a))!=null)
                throw new IllegalStateException("unreviewed ownership at "+toAddr(a));
        }
        int transaction=currentProgram.startTransaction("recover pinned byte-mask dispatcher");
        boolean commit=false;
        try {
            Set<Long> starts=new HashSet<>(), targets=new HashSet<>();
            for(long cursor=ENTRY;cursor<END;) {
                Instruction instruction=getInstructionAt(toAddr(cursor));
                if(instruction==null) {
                    if(getInstructionContaining(toAddr(cursor))!=null||
                       !new DisassembleCommand(toAddr(cursor),body,false).applyTo(currentProgram,monitor))
                        throw new IllegalStateException("cannot decode at "+toAddr(cursor));
                    instruction=getInstructionAt(toAddr(cursor));
                }
                if(instruction==null||cursor+instruction.getLength()>END)
                    throw new IllegalStateException("instruction crosses body boundary");
                starts.add(cursor);
                var flow=instruction.getFlowType();
                if(flow.isCall()||(flow.isComputed()&&cursor!=JUMP)||
                   (flow.isTerminal()&&cursor!=END-1&&cursor!=JUMP))
                    throw new IllegalStateException("unexpected transfer at "+toAddr(cursor));
                for(var target:instruction.getFlows()) {
                    if(target.getOffset()<ENTRY||target.getOffset()>=END)
                        throw new IllegalStateException("external branch");
                    targets.add(target.getOffset());
                }
                var fallthrough=instruction.getFallThrough();
                if(fallthrough!=null&&(fallthrough.getOffset()<ENTRY||fallthrough.getOffset()>=END))
                    throw new IllegalStateException("external fallthrough");
                cursor+=instruction.getLength();
            }
            Instruction jump=getInstructionAt(toAddr(JUMP)), ret=getInstructionAt(toAddr(END-1));
            if(jump==null||!jump.getFlowType().isComputed()||!jump.getFlowType().isJump()||
               ret==null||!ret.getMnemonicString().equals("RET")||ret.getNumOperands()!=0)
                throw new IllegalStateException("missing dispatch or terminal RET");
            Set<Long> cases=new HashSet<>();
            for(int i=0;i<256;++i) {
                long target=Integer.toUnsignedLong(currentProgram.getMemory().getInt(toAddr(TABLE+4*i)));
                if(target<0x11db46||target>0x11e2fa||!starts.contains(target)||!cases.add(target))
                    throw new IllegalStateException("invalid or duplicate case target");
                currentProgram.getReferenceManager().addMemoryReference(toAddr(JUMP),toAddr(target),
                    RefType.COMPUTED_JUMP,SourceType.USER_DEFINED,0);
            }
            if(!starts.containsAll(targets)) throw new IllegalStateException("branch into instruction interior");
            if(function==null) function=createFunction(toAddr(ENTRY),"draw_byte_mask_11dab0");
            if(function==null) throw new IllegalStateException("cannot create function");
            function.setBody(body);
            if(!function.getBody().equals(body)) throw new IllegalStateException("incomplete body");
            Files.writeString(Path.of(args[0]),"entry\tbody_bytes\tinstructions\tcases\tresult\n"+
                String.format("0x%08x\t%d\t%d\t%d\t%s\n",ENTRY,END-ENTRY,starts.size(),cases.size(),args[1]));
            commit=args[1].equals("apply");
        } finally { currentProgram.endTransaction(transaction,commit); }
    }
}
