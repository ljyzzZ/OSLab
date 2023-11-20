#include "klib.h"
#include "serial.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "fs.h"
#include "proc.h"
#include "timer.h"
#include "dev.h"

void init_user_and_go();

int main()
{
  init_gdt();
  init_serial();
  init_fs();
  init_page();  // uncomment me at Lab1-4
  init_cte();   // uncomment me at Lab1-5
  init_timer(); // uncomment me at Lab1-7
  init_proc();  // uncomment me at Lab2-1
  init_dev(); // uncomment me at Lab3-1
  printf("Hello from OS!\n");
  init_user_and_go();
  panic("should never come back");
}

void init_user_and_go()
{
  // Lab1-2: ((void(*)())eip)();
  // Lab1-4: pdgir, stack_switch_call
  // Lab1-6: ctx, irq_iret
  // Lab1-8: argv
  // Lab2-1: proc
  // Lab3-2: add cwd

  // lab1-2:将名字为name的用户程序加载到内存，返回其入口地址（或-1如果不存在这个用户程序），第一个参数现在没有意义
  /*uint32_t eip = load_elf(NULL, "loaduser");
  assert(eip != -1);
  ((void (*)())eip)();*/

  // lab1-4:
  /*PD *pgdir = vm_alloc();
  uint32_t eip = load_elf(pgdir, "systest");
  assert(eip != -1);
  set_cr3(pgdir);
  stack_switch_call((void *)(USR_MEM - 16), (void *)eip, 0);*/

  // lab1-6:
  /*PD *pgdir = vm_alloc();
  Context ctx;
  // char *argv[] = {"echo", "hello", "world", NULL};
  assert(load_user(pgdir, &ctx, "sh1", NULL) == 0);
  set_cr3(pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)kalloc() + PGSIZE);
  irq_iret(&ctx);*/

  // lab2-1:
  /*proc_t *proc = proc_alloc();
  assert(proc);
  char *argv[] = {"sh1", NULL};
  assert(load_user(proc->pgdir, proc->ctx, "sh1", argv) == 0);
  proc_addready(proc);
  while (1)
    proc_yield();*/
  // lab2-1: 让操作系统加载两个用户进程
  /*proc_t *proc = proc_alloc();
  assert(proc);
  char *argv[] = {"ping2", "114514", NULL};
  assert(load_user(proc->pgdir, proc->ctx, "ping2", argv) == 0);
  proc_addready(proc);

  proc = proc_alloc();
  assert(proc);
  argv[1] = "1919810";
  assert(load_user(proc->pgdir, proc->ctx, "ping2", argv) == 0);
  proc_addready(proc);

  sti();
  while (1)
    ;*/
  // lab2-1
  proc_t *proc = proc_alloc();
  assert(proc);
  char *argv[] = {"sh", NULL};
  assert(load_user(proc->pgdir, proc->ctx, "sh", argv) == 0);
  proc->cwd = iopen("/", TYPE_NONE);
  proc_addready(proc);
  sti();
  while (1)
    ;
}
