// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception

// Isolated Linux compatibility-mode ENTER fault-ordering probe.
#define _GNU_SOURCE
#include <asm/ldt.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <signal.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static sigjmp_buf resume;
static volatile unsigned long ip,sp,bp,trap,error,selectors;
static volatile int caught;
static void handler(int sig, siginfo_t *info, void *context) {
  ucontext_t *u=context; (void)info;
  caught=sig; ip=u->uc_mcontext.gregs[REG_RIP];sp=u->uc_mcontext.gregs[REG_RSP];bp=u->uc_mcontext.gregs[REG_RBP];
  trap=u->uc_mcontext.gregs[REG_TRAPNO];error=u->uc_mcontext.gregs[REG_ERR];selectors=u->uc_mcontext.gregs[REG_CSGSFS];
  siglongjmp(resume,1);
}
int main(int argc,char **argv) {
  unsigned allocation=argc>1?strtoul(argv[1],0,0):8;
  unsigned char *mem=mmap(0,0x4000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
  unsigned char *code=mmap(0,0x1000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
  if(mem==MAP_FAILED||code==MAP_FAILED)return 2;
  uintptr_t initial=(uintptr_t)mem+0x1004;
  struct user_desc desc={0};desc.entry_number=0;desc.base_addr=0;desc.limit=((uintptr_t)mem+0xfff)>>12;
  desc.seg_32bit=1;desc.contents=1;desc.limit_in_pages=1;desc.useable=1;
  if(syscall(SYS_modify_ldt,1,&desc,sizeof(desc))<0){perror("modify_ldt");return 2;}
  stack_t alt={.ss_sp=malloc(65536),.ss_size=65536};if(sigaltstack(&alt,0))return 2;
  struct sigaction action={0};action.sa_sigaction=handler;action.sa_flags=SA_SIGINFO|SA_ONSTACK;
  sigemptyset(&action.sa_mask);sigaction(SIGSEGV,&action,0);sigaction(SIGBUS,&action,0);sigaction(SIGTRAP,&action,0);
  unsigned char bytes[]={0x66,0xb8,7,0,0x8e,0xd0,0xbc,0,0,0,0,0xbd,0x4d,0,0,0,0xc8,0,0,0,0xcc};
  uint32_t esp=initial;memcpy(bytes+7,&esp,4);bytes[17]=allocation;bytes[18]=allocation>>8;memcpy(code,bytes,sizeof(bytes));
  if(!sigsetjmp(resume,1)) {
    __asm__ volatile("pushq $0x23\n\tpushq %0\n\tlretq"::"r"(code):"memory");
  }
  uint32_t saved;memcpy(&saved,(void*)(initial-4),4);
  printf("{\"allocation\":%u,\"initial_esp\":%lu,\"limit\":%lu,\"signal\":%d,\"ip_offset\":%ld,\"esp\":%lu,\"ebp\":%lu,\"trap\":%lu,\"error\":%lu,\"selectors\":%lu,\"saved_ebp\":%u}\n",allocation,initial,(uintptr_t)mem+0xfff,caught,(long)(ip-(uintptr_t)code),sp,bp,trap,error,selectors,saved);
  return 0;
}
