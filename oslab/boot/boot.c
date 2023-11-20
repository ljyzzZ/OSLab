#include "boot.h"

// DO NOT DEFINE ANY NON-LOCAL VARIBLE!

void load_kernel()
{
  /*char hello[] = {'\n', 'h', 'e', 'l', 'l', 'o', '\n', 0};
  putstr(hello);
  while (1) ;*/
  // remove both lines above before write codes below
  Elf32_Ehdr *elf = (void *)0x8000; // 把整个ELF文件读到内存0x8000处
  copy_from_disk(elf, 255 * SECTSIZE, SECTSIZE);
  Elf32_Phdr *ph, *eph;                        // 程序头表
  ph = (void *)((uint32_t)elf + elf->e_phoff); // 程序头表在ELF文件中的偏移量
  eph = ph + elf->e_phnum;                     // 程序头表中包含表项的个数
  for (; ph < eph; ph++)
  {
    if (ph->p_type == PT_LOAD)
    {
      // TODO: Lab1-2, Load kernel and jump
      memcpy((void *)ph->p_vaddr, (const void *)((void *)0x8000 + ph->p_offset), ph->p_filesz); // 注意p_offset是相对于ELF文件的偏移量，要加上ELF文件起始内存地址，下面同理
      if (ph->p_memsz > ph->p_filesz)
        memset((void *)((void *)0x8000 + ph->p_memsz - ph->p_filesz), (int)0, ph->p_memsz - ph->p_filesz);
    }
  }
  uint32_t entry = (uint32_t)elf->e_entry; // change me
  ((void (*)())entry)();
}
