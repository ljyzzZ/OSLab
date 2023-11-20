#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx)
{
  // TODO: Lab1-5 call specific syscall handle and set ctx register
  int sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS)
  {
    res = -1;
  }
  else
  {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count)
{
  // TODO: rewrite me at Lab3-1
  proc_t* curr = proc_curr();
  file_t* write_file = proc_getfile(curr, fd);
  if(write_file == NULL)
    return -1;
  return fwrite(write_file, buf, count);
  // return serial_write(buf, count);
}

int sys_read(int fd, void *buf, size_t count)
{
  // TODO: rewrite me at Lab3-1
  proc_t* curr = proc_curr();
  file_t* read_file = proc_getfile(curr, fd);
  if(read_file == NULL)
    return -1;
  return fread(read_file, buf, count);
  // return serial_read(buf, count);
}

int sys_brk(void *addr)
{
  // TODO: Lab1-5
  // static size_t pcb->brk = 0; // use pcb->brk of proc instead of this in Lab2-1
  proc_t *pcb = proc_curr();
  size_t new_brk = PAGE_UP(addr);
  if (pcb->brk == 0)
  {
    pcb->brk = new_brk;
  }
  else if (new_brk > pcb->brk)
  {
    PD *pgdir = vm_curr();
    vm_map(pgdir, pcb->brk, new_brk - pcb->brk, 7);
    pcb->brk = new_brk;
  }
  else if (new_brk < pcb->brk)
  {
    // can just do nothing
  }
  return 0;
}

void sys_sleep(int ticks)
{
  uint32_t now = get_tick();
  while ((get_tick() - now) < ticks)
  {
    proc_yield();
  }
}

int sys_exec(const char *path, char *const argv[])
{
  // Lab1-8, Lab2-1
  PD *pgdir = vm_alloc();
  Context ctx;
  if (load_user(pgdir, &ctx, path, argv) == 0)
  {
    PD *temp = vm_curr();
    proc_t *pcb = proc_curr();
    pcb->pgdir = pgdir;
    set_cr3(pgdir);
    kfree(temp);
    irq_iret(&ctx);
  }
  else
  {
    kfree(pgdir);
    return -1;
  }
}

int sys_getpid()
{
  proc_t *pcb = proc_curr(); // Lab2-1
  return pcb->pid;
}

void sys_yield()
{
  proc_yield();
}

int sys_fork()
{
  // Lab2-2
  proc_t *pcb = proc_alloc();
  if (pcb == NULL)
    return -1;
  proc_copycurr(pcb);
  proc_addready(pcb);
  return pcb->pid;
}

void sys_exit(int status)
{
  // Lab2-3
  proc_t *curr = proc_curr();
  proc_makezombie(curr, status);
  INT(0x81);
  assert(0);
}

int sys_wait(int *status)
{
  // Lab2-3, Lab2-4
  proc_t *curr = proc_curr();
  if (curr->child_num == 0)
    return -1;
  sem_p(&curr->zombie_sem);
  proc_t *zombie;
  zombie = proc_findzombie(curr);
  /* while ((zombie = proc_findzombie(curr)) == NULL)
  {
    proc_yield();
  }*/
  if (status != NULL)
    *status = zombie->exit_code;
  int pid = zombie->pid;
  proc_free(zombie);
  curr->child_num--;
  return pid;
}

int sys_sem_open(int value)
{
  // Lab2-5
  proc_t *curr = proc_curr();
  int sem_id = proc_allocusem(curr);
  usem_t *usem = usem_alloc(value);
  if (sem_id == -1 || usem == NULL)
    return -1;
  curr->usems[sem_id] = usem;
  return sem_id;
}

int sys_sem_p(int sem_id)
{
  // Lab2-5
  proc_t *curr = proc_curr();
  usem_t *usem = proc_getusem(curr, sem_id);
  if (usem == NULL)
    return -1;
  sem_p(&usem->sem);
  return 0;
}

int sys_sem_v(int sem_id)
{
  // Lab2-5
  proc_t *curr = proc_curr();
  usem_t *usem = proc_getusem(curr, sem_id);
  if (usem == NULL)
    return -1;
  sem_v(&usem->sem);
  return 0;
}

int sys_sem_close(int sem_id)
{
  // Lab2-5
  proc_t *curr = proc_curr();
  usem_t *usem = proc_getusem(curr, sem_id);
  if (usem == NULL)
    return -1;
  usem_close(usem);
  curr->usems[sem_id] = NULL;
  return 0;
}

int sys_open(const char *path, int mode)
{
  // Lab3-1
  proc_t* curr = proc_curr();
  int num = proc_allocfile(curr);
  if(num == -1)
    return -1;
  curr->files[num] = fopen(path, mode);
  if(curr->files[num] == NULL)
    return -1;
  return num;
}

int sys_close(int fd)
{
  // Lab3-1
  proc_t* curr = proc_curr();
  file_t* close_file = proc_getfile(curr, fd);
  if(close_file == NULL)
    return -1;
  fclose(close_file);
  curr->files[fd] = NULL;
  return 0;
}

int sys_dup(int fd)
{
  // Lab3-1
  proc_t* curr = proc_curr();
  int vacancy = proc_allocfile(curr);
  file_t* dup_file = proc_getfile(curr, fd);
  if(vacancy == -1 || dup_file == NULL)
    return -1;
  curr->files[vacancy] = dup_file;
  fdup(dup_file);
  return vacancy;
}

uint32_t sys_lseek(int fd, uint32_t off, int whence)
{
  // Lab3-1
  proc_t* curr = proc_curr();
  file_t* seek_file = proc_getfile(curr, fd);
  if(seek_file == NULL)
    return -1;
  return fseek(seek_file, off, whence);
}

int sys_fstat(int fd, struct stat *st)
{
  // Lab3-1
  proc_t* curr = proc_curr();
  file_t* stat_file = proc_getfile(curr, fd);
  if(stat_file == NULL)
    return -1;
  if(stat_file->type == TYPE_FILE || stat_file->type == TYPE_DIR){
    st->type = itype(stat_file->inode);
    st->size = isize(stat_file->inode);
    st->node = ino(stat_file->inode);
    return 0;
  }
  else if(stat_file->type == TYPE_DEV){
    st->type = TYPE_DEV;
    st->size = 0;
    st->node = 0;
    return 0;
  }
  return -1;
}

int sys_chdir(const char *path)
{
  // Lab3-2
  inode_t* change_file = iopen(path, TYPE_NONE);
  if(change_file == NULL)
    return -1;
  if(itype(change_file) != TYPE_DIR){
    iclose(change_file);
    return -1;
  }
  proc_t* curr = proc_curr();
  iclose(curr->cwd);
  curr->cwd = change_file;
  return 0;
}

int sys_unlink(const char *path)
{
  return iremove(path);
}

// optional syscall

void *sys_mmap()
{
  TODO();
}

void sys_munmap(void *addr)
{
  TODO();
}

int sys_clone(void (*entry)(void *), void *stack, void *arg)
{
  TODO();
}

int sys_kill(int pid)
{
  TODO();
}

int sys_cv_open()
{
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id)
{
  TODO();
}

int sys_cv_sig(int cv_id)
{
  TODO();
}

int sys_cv_sigall(int cv_id)
{
  TODO();
}

int sys_cv_close(int cv_id)
{
  TODO();
}

int sys_pipe(int fd[2])
{
  TODO();
}

int sys_link(const char *oldpath, const char *newpath)
{
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath)
{
  TODO();
}

void *syscall_handle[NR_SYS] = {
    [SYS_write] = sys_write,
    [SYS_read] = sys_read,
    [SYS_brk] = sys_brk,
    [SYS_sleep] = sys_sleep,
    [SYS_exec] = sys_exec,
    [SYS_getpid] = sys_getpid,
    [SYS_yield] = sys_yield,
    [SYS_fork] = sys_fork,
    [SYS_exit] = sys_exit,
    [SYS_wait] = sys_wait,
    [SYS_sem_open] = sys_sem_open,
    [SYS_sem_p] = sys_sem_p,
    [SYS_sem_v] = sys_sem_v,
    [SYS_sem_close] = sys_sem_close,
    [SYS_open] = sys_open,
    [SYS_close] = sys_close,
    [SYS_dup] = sys_dup,
    [SYS_lseek] = sys_lseek,
    [SYS_fstat] = sys_fstat,
    [SYS_chdir] = sys_chdir,
    [SYS_unlink] = sys_unlink,
    [SYS_mmap] = sys_mmap,
    [SYS_munmap] = sys_munmap,
    [SYS_clone] = sys_clone,
    [SYS_kill] = sys_kill,
    [SYS_cv_open] = sys_cv_open,
    [SYS_cv_wait] = sys_cv_wait,
    [SYS_cv_sig] = sys_cv_sig,
    [SYS_cv_sigall] = sys_cv_sigall,
    [SYS_cv_close] = sys_cv_close,
    [SYS_pipe] = sys_pipe,
    [SYS_link] = sys_link,
    [SYS_symlink] = sys_symlink};
