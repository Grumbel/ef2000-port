// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Isolated repeated comparison fault-state probe; no original game code.
#define _GNU_SOURCE
#include <sys/mman.h>
#include <signal.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static sigjmp_buf resume;
static volatile unsigned long flags,si,di,cx,trap,err,selectors,cr2;
static void handler(int sig,siginfo_t *info,void *context){
 ucontext_t *u=context;(void)sig;(void)info;
 cr2=u->uc_mcontext.gregs[REG_CR2];flags=u->uc_mcontext.gregs[REG_EFL];si=u->uc_mcontext.gregs[REG_RSI];di=u->uc_mcontext.gregs[REG_RDI];cx=u->uc_mcontext.gregs[REG_RCX];trap=u->uc_mcontext.gregs[REG_TRAPNO];err=u->uc_mcontext.gregs[REG_ERR];selectors=u->uc_mcontext.gregs[REG_CSGSFS];siglongjmp(resume,1);
}
int main(int argc,char **argv){
 int scan=argc>1?atoi(argv[1]):0;
 int mode=argc>2?atoi(argv[2]):0;
 unsigned char *src=mmap(0,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 unsigned char *dst=mmap(0,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 if(src==MAP_FAILED||dst==MAP_FAILED)return 2;
 memset(src,0,8192);memset(dst,0,8192);if(mprotect(dst+4096,4096,PROT_NONE))return 2;
 struct sigaction a={0};a.sa_sigaction=handler;a.sa_flags=SA_SIGINFO;sigemptyset(&a.sa_mask);sigaction(SIGSEGV,&a,0);
 unsigned char *code=mmap(0,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
 if(code==MAP_FAILED)return 2;
 stack_t alt={.ss_sp=malloc(65536),.ss_size=65536};if(!alt.ss_sp||sigaltstack(&alt,0))return 2;
 a.sa_flags=SA_SIGINFO|SA_ONSTACK;sigaction(SIGSEGV,&a,0);
 /* Linux user compatibility CS; use an isolated low stack and alternate
  * signal stack. Native instruction execution stays entirely in this process. */
 unsigned char bytes[]={0xbc,0,0,0,0,0xbe,0,0,0,0,0xbf,0,0,0,0,
                        0xb9,4,0,0,0,0xb8,0,0,0,0,0x68,2,2,0,0,0x9d,0xf3,0xa6,0xcc};
 uint32_t stack=(uintptr_t)src+8192,source=(uintptr_t)src,destination=(uintptr_t)dst+4094;
 /* Competing first-iteration faults: both pages, source page + null ES,
  * and null DS + destination page. Keep the isolated stack mapped. */
 if(mode==2||mode==4)destination=(uintptr_t)dst+4096;
 if((mode==2||mode==3)&&mprotect(src,4096,PROT_NONE))return 2;
 memcpy(bytes+1,&stack,4);memcpy(bytes+6,&source,4);memcpy(bytes+11,&destination,4);
 if(scan)bytes[32]=0xae;
 const unsigned char segments[]={0x66,0xb8,0x2b,0,0x8e,0xd8,0x8e,0xc0,0x8e,0xd0};
 memcpy(code,segments,sizeof(segments));
 unsigned length=sizeof(segments);
 if(mode==3||mode==4){
  unsigned char nullseg[]={0x66,0xb8,0,0,0x8e,0xc0};
  if(mode==4)nullseg[5]=0xd8;
  memcpy(code+length,nullseg,sizeof(nullseg));length+=sizeof(nullseg);
 }
 memcpy(code+length,bytes,sizeof(bytes));
 if(!sigsetjmp(resume,1)){
  __asm__ volatile("pushq $0x23\n\tpushq %0\n\tlretq"::"r"(code):"memory");
 }
 printf("{\"scan\":%d,\"mode\":%d,\"flags\":%lu,\"si_delta\":%ld,\"di_delta\":%ld,\"count\":%lu,\"trap\":%lu,\"error\":%lu,\"selectors\":%lu,\"fault_source\":%d,\"fault_destination\":%d}\n",scan,mode,flags,(long)(si-source),(long)(di-destination),cx,trap,err,selectors,cr2==source,cr2==destination);
 return 0;
}
