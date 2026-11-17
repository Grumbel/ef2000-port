// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
// @category EF2000
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.HexFormat;

/** Restore reviewed terminal RET instructions excluded from seven bodies. */
public class EF2000RecoverReturnTails extends GhidraScript {
 private String hash(long start,int length) throws Exception {
  byte[] bytes=new byte[length];
  if(currentProgram.getMemory().getBytes(toAddr(start),bytes)!=length) throw new IllegalStateException("short image read");
  return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
 }
 protected void run() throws Exception {
  String[] args=getScriptArgs();
  if(args.length!=2||(!args[1].equals("apply")&&!args[1].equals("report"))) throw new IllegalArgumentException("output.tsv, report|apply");
  if(!currentProgram.getLanguageID().toString().equals("x86:LE:32:default")||!hash(0,0x195fb0).equals("3fd26b189efe289feb9e6ac701d592a5d48d7c1e152527aa00c01611e87e0fdb")) throw new IllegalStateException("unexpected image");
  StringBuilder report=new StringBuilder("entry\tbody_bytes_before\tbody_bytes_after\n");
  int transaction=currentProgram.startTransaction("reviewed return tails");boolean commit=false;
  try {
   {
   long entry=0x2065c,tail=0x20afa;int length=3;
   if(!hash(tail,length).equals("e598d0c3ba86d917b177d7adde0556aa99bc355543c57aee0a3e50b684dd7e99")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x2065c),toAddr(0x20af9));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   {
   long entry=0x34a98,tail=0x34ae4;int length=1;
   if(!hash(tail,length).equals("ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x34a98),toAddr(0x34ae3));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   {
   long entry=0x363c4,tail=0x36414;int length=1;
   if(!hash(tail,length).equals("ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x363c4),toAddr(0x36413));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   {
   long entry=0x4d5f0,tail=0x4d654;int length=1;
   if(!hash(tail,length).equals("ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x4d5f0),toAddr(0x4d653));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   {
   long entry=0x4dae8,tail=0x4db4c;int length=1;
   if(!hash(tail,length).equals("ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x4dae8),toAddr(0x4db4b));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   {
   long entry=0x5086c,tail=0x50b34;int length=1;
   if(!hash(tail,length).equals("ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x5086c),toAddr(0x5089a));
   old.add(toAddr(0x50a52),toAddr(0x50b33));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   {
   long entry=0x68c18,tail=0x68c94;int length=1;
   if(!hash(tail,length).equals("ae3f4619b0413d70d3004b9131c3752153074e45725be13b9a148978895e359e")) throw new IllegalStateException("unexpected return bytes");
   Function function=getFunctionAt(toAddr(entry));AddressSet old=new AddressSet();
   old.add(toAddr(0x68c18),toAddr(0x68c93));
   AddressSet body=new AddressSet(old);body.add(toAddr(tail),toAddr(tail+length-1));
   Instruction previous=getInstructionContaining(toAddr(tail-1));
   if(function==null||(!function.getBody().equals(old)&&!function.getBody().equals(body))||previous==null||!toAddr(tail).equals(previous.getFallThrough())) throw new IllegalStateException("unexpected body/fallthrough");
   for(long k=tail;k<tail+length;k++) { Function owner=getFunctionContaining(toAddr(k));if(owner!=null&&owner!=function) throw new IllegalStateException("other owner"); }
   long before=function.getBody().getNumAddresses();
   clearListing(toAddr(tail),toAddr(tail+length-1));
   if(!disassemble(toAddr(tail))) throw new IllegalStateException("disassembly failed");
   Instruction ret=getInstructionAt(toAddr(tail));
   if(ret==null||!ret.getMnemonicString().equals("RET")||ret.getLength()!=length||ret.getFallThrough()!=null) throw new IllegalStateException("unexpected return");
   function.setBody(body);
   report.append(String.format("0x%08x\t%d\t%d\n",entry,before,body.getNumAddresses()));
   }
   Files.writeString(Path.of(args[0]),report.toString());commit=args[1].equals("apply");
  } finally {currentProgram.endTransaction(transaction,commit);}
 }
}
