#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

typedef union free_page
{
  union free_page *next; // 前4个字节
  char buf[PGSIZE];      // 4096, 刚好是一个页
} page_t;

page_t *free_page_list;

void init_gdt()
{
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R, 0, 0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W, 0, 0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W, 0, 0xffffffff, DPL_USER);
  gdt[SEG_TSS] = SEG16(STS_T32A, &tss, sizeof(tss) - 1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0)
{
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));
void *free_page_addr;

void init_page()
{
  extern char end; // end的地址：kernel的静态内存的顶端
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
  for (int i = 0; i < PHY_MEM / PT_SIZE; i++) // PT_SIZE:0X00400000
  {
    kpd.pde[i].val = MAKE_PDE(&kpt[i], 1); // 把kpd的第i个PDE对应到kpt[i]这个页表
    for (int j = 0; j < NR_PTE; j++)
    {
      kpt[i].pte[j].val = MAKE_PTE(((i << DIR_SHIFT) | (j << TBL_SHIFT)), 1);
    }
  }
  kpt[0].pte[0].val = 0;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);
  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
  // 采用空闲页单链表的写法
  free_page_list = (page_t *)KER_MEM;
  page_t *p = free_page_list;
  for (int addr = KER_MEM + PGSIZE; addr < PHY_MEM; addr += PGSIZE)
  {
    p->next = (page_t *)addr;
    p = p->next;
  }
  p->next = NULL;

  // 采用单指针写法
  // free_page_addr = (void *)KER_MEM;
}

// 分配一页（4KiB）内存并返回其地址，要求分配出的内存按页对齐
void *kalloc() // 链表头删
{
  // Lab1-4: alloc a page from kernel heap, abort when heap empty
  // 空闲页单链表
  if (free_page_list->next == NULL)
  { // kalloc没有空闲内存时assert(0)
    Log("no free page\n");
    assert(0);
  }
  else
  {
    page_t *new_page = free_page_list;
    free_page_list = free_page_list->next;
    /*page_t *delete_page = free_page_list->next;
      free_page_list->next = delete_page->next;
      PT *new_page = (PT *)delete_page;*/
    // 要求按页对齐所以用PT对一下?
    return (PT *)new_page;
  }
  // 单指针
  /*if ((int)free_page_addr + PGSIZE >= PHY_MEM)
    assert(0);
  void *temp = free_page_addr;
  free_page_addr += PGSIZE;
  return temp;*/
}

// 回收ptr指向的那一页内存，要求ptr指向之前分配出去的某一页
void kfree(void *ptr) // 链表头插
{
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  // TODO();
  page_t *new_page = (page_t *)ptr;
  page_t *next = free_page_list->next;
  free_page_list->next = new_page;
  new_page->next = next;
}

PD *vm_alloc()
{
  // Lab1-4: alloc a new pgdir, map memory under PHY_MEM identityly
  PD *new_dir = (PD *)PAGE_DOWN(kalloc()); // 是否需要PAGE_DOWN?
  for (int i = 0; i < NR_PDE; i++)
  {
    if (i < 32)
      new_dir->pde[i].val = MAKE_PDE(&kpt[i], 1); // 把new_dir的第i个PDE(前32个)对应到kpt[i]这个页表
    else
      new_dir->pde[i].val = 0;
  }
  return new_dir;
}

void vm_teardown(PD *pgdir)
{
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  // TODO();
}

PD *vm_curr()
{
  return (PD *)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot)
{
  // Lab1-4: return the pointer of PTE which match va
  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // remember to let pde's prot |= prot, but not pte
  assert((prot & ~7) == 0);
  int pd_index = ADDR2DIR(va);        // 计算“页目录号”
  PDE *pde = &(pgdir->pde[pd_index]); // 找到对应的页目录项
  if (pde->present == 0)              // 有效位为0, 对应pte不存在
  {
    if (!(prot & 1))
      return NULL;
    else if (prot & 1) // kalloc一页作为页表并清零，用prot作为权限设置这个PDE
    {
      PT *pt = (PT *)PAGE_DOWN(kalloc());
      for (int i = 0; i < NR_PTE; i++)
        pt->pte[i].val = 0;
      pde->val = MAKE_PDE(pt, prot);
      int pt_index = ADDR2TBL(va);     // 计算“页表号”
      PTE *pte = &(pt->pte[pt_index]); // 找到对应的页表项
      return pte;
    }
    else
      return NULL;
  }
  else
  {
    PT *pt = PDE2PT(*pde);           // 根据PDE找页表的地址
    int pt_index = ADDR2TBL(va);     // 计算“页表号”
    PTE *pte = &(pt->pte[pt_index]); // 找到对应的页表项
    return pte;
  }
}

void *vm_walk(PD *pgdir, size_t va, int prot)
{
  // Lab1-4: translate va to pa
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  // if va is not mapped and !(prot&1), return NULL
  PTE *pte = vm_walkpte(pgdir, va, prot);
  if (pte == NULL) // 如果没有被映射返回NULL
    return NULL;
  void *page = PTE2PG(*pte);                          // 根据PTE找物理页的地址
  void *pa = (void *)((uint32_t)page | ADDR2OFF(va)); // 补上页内偏移量
  return pa;
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot)
{
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  assert(prot & PTE_P);
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);
  for (size_t vaddr = start; vaddr < end; vaddr += PGSIZE)
  {
    PTE *pte = vm_walkpte(pgdir, vaddr, prot); // 返回pgdir这一页目录下，指向va这一虚拟地址对应的PTE的指针
    if (pte->present & 1)                      // 其中包含已经被映射的虚拟页的时候，可以直接忽略
      continue;
    size_t paddr = PAGE_DOWN(kalloc());
    pte->val = MAKE_PTE(paddr, prot);
  }
}

void vm_unmap(PD *pgdir, size_t va, size_t len)
{
  // Lab1-4: unmap and free [va, va+len) at pgdir
  // you can just do nothing :)
  // assert(ADDR2OFF(va) == 0);
  // assert(ADDR2OFF(len) == 0);
  // TODO();
}

void vm_copycurr(PD *pgdir)
{
  // Lab2-2: copy memory mapped in curr pd to pgdir
  for (size_t vaddr = PHY_MEM; vaddr < USR_MEM; vaddr += PGSIZE)
  {
    proc_t *curr = proc_curr();
    PTE *pte = vm_walkpte(curr->pgdir, vaddr, 0);
    if ((pte != NULL) && (pte->present != 0))
    {
      vm_map(pgdir, vaddr, PGSIZE, 7);
      void *paddr = vm_walk(pgdir, vaddr, 7);
      memcpy(paddr, (const void *)vaddr, PGSIZE);
    }
  }
}

void vm_pgfault(size_t va, int errcode)
{
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
