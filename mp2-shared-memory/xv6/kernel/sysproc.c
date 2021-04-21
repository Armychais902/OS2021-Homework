#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "fs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fcntl.h"
#include "file.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  // myproc()->sz += n;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

/* remove needed ? */
// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// assume length will be multiple of PGSIZE
uint64
sys_mmap(void)
{
  uint64 addr;
  int length;
  int prot;
  int flags;
  int fd;
  int offset;
  struct file *fp;

  // get the args from trapframe
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0 || argint(3, &flags) < 0 || argfd(4, &fd, &fp) < 0 || argint(5, &offset) < 0)
    return -1;
  
  /* addr and offset is assumed to be 0:
      addr = 0, means select pa freely
   */

  struct proc *p = myproc();

  // if map to a unwritable file with PROT_WRITE
  uint64 pte_flags = PTE_U;
  if (prot & PROT_WRITE)
  {
    if (!fp->writable && (flags & MAP_SHARED))
      return -1;
    pte_flags |= PTE_W;
  }
  if (prot & PROT_READ)
  {
    if (!fp->readable)
      return -1;
    pte_flags |= PTE_R;
  }
  
  struct VMA *tmp_vma = 0;
  for (int i = 0; i < nVMA; i++)
  {
    if (p->vma_list[i].vm_valid == 1)
    {
      tmp_vma = &p->vma_list[i];
      break;
    }
  }
  if (tmp_vma != 0)
  {
    tmp_vma->vm_valid = 0;
    tmp_vma->vm_fd = fd;
    tmp_vma->vm_file = fp;
    tmp_vma->vm_flags = flags;
    tmp_vma->vm_page_prot = pte_flags;
    tmp_vma->offset = offset;
    // vm_end align
    tmp_vma->vm_end = PGROUNDDOWN(p->curr_va);
    // to guarantee that end - start >= length
    tmp_vma->vm_start = PGROUNDDOWN(tmp_vma->vm_end - length);
    // printf("start %p end %p\n", tmp_vma->vm_start, tmp_vma->vm_end);
    p->curr_va = tmp_vma->vm_start;
    fp = filedup(fp);
  }
  else
    return -1;
  return tmp_vma->vm_start;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  int length;
  // assume length of munmap will be multiple of PGSIZE
  // assume address will be multiple of PGSIZE
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  struct proc *p = myproc();

  // assume it won't cross different vma and will munmap at start or toward end point (maybe not the whole)
  for (int i = 0; i < nVMA; i++)
  {
    // addr should not at vm_end
    if (!p->vma_list[i].vm_valid && p->vma_list[i].vm_start <= addr && addr < p->vma_list[i].vm_end)
    {
      struct VMA *vma_tmp = &(p->vma_list[i]);
      if (walkaddr(p->pagetable, addr) > 0)
      {
        if (vma_tmp->vm_flags == MAP_SHARED)
          filewrite(vma_tmp->vm_file, addr, length);
        uvmunmap(p->pagetable, addr, PGROUNDUP(length)/PGSIZE, 1);
      }
      // now vm_start still page-aligned
      if (addr == vma_tmp->vm_start)
        vma_tmp->vm_start += length;
      else
        vma_tmp->vm_end -= length;
      
      // now vm_start or vm_end still page-aligned
      if (vma_tmp->vm_start == vma_tmp->vm_end)
      {
        vma_tmp->vm_file->ref--;
        vma_tmp->vm_valid = 1;
      }

      // assume that the memory is unlimited, so curr_va pointer could always move downward
      int j = 0;
      for (; j < nVMA; j++)
      {
        if (!p->vma_list[j].vm_valid)
          break;
      }
      // if no invalid vma, initialize the pointer to VMA_strt
      if (j == nVMA)
        p->curr_va = VMA_strt;
      return 0;
    }
  }
  return -1;
}