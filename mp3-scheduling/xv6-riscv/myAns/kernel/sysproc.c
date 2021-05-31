#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
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
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
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

  if (argint(0, &pid) < 0)
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

// for mp3
// assume the assigned context_id hasn't been used
uint64
sys_thrdstop(void)
{
  int interval, thrdstop_context_id;
  uint64 handler;
  if (argint(0, &interval) < 0)
    return -1;
  if (argint(1, &thrdstop_context_id) < 0)
    return -1;
  if (argaddr(2, &handler) < 0)
    return -1;

  struct proc *p = myproc();
  p->thrdstop_ticks = 0;
  p->thrdstop_interval = interval;
  p->thrdstop_handler_pointer = handler;

  if (thrdstop_context_id == -1)
  {
    int i = 0;
    for (i = 0; i < MAX_THRD_NUM; i++)
    {
      if (p->thrdstop_context_used[i] == 0)
      {
        p->thrdstop_context_id = i;
        p->thrdstop_context_used[i] = 1;
        return i;
      }
    }
    if (i == MAX_THRD_NUM)
      return -1;
  }
  else
  {
    p->thrdstop_context_id = thrdstop_context_id;
    p->thrdstop_context_used[thrdstop_context_id] = 1;
    return thrdstop_context_id;
  }
  return 0;
}

// for mp3
// assume context_id hasn't been used
uint64
sys_cancelthrdstop(void)
{
  int thrdstop_context_id;
  if (argint(0, &thrdstop_context_id) < 0)
    return -1;
  
  struct proc *p = myproc();
  int count = p->thrdstop_ticks;
  p->thrdstop_ticks = 0;
  p->thrdstop_interval = -1;
  p->thrdstop_context_id = -1;
  p->thrdstop_handler_pointer = 0;
  
  if (thrdstop_context_id != -1)
  {
    struct thrd_context_data *stored = &(p->thrdstop_context[thrdstop_context_id]);
    stored->ra = p->trapframe->ra;
    stored->gp = p->trapframe->gp;
    stored->sp = p->trapframe->sp;
    stored->tp = p->trapframe->tp;
    stored->epc = p->trapframe->epc;
    /* s_regs */
    stored->s_regs[0] = p->trapframe->s0;
    stored->s_regs[1] = p->trapframe->s1;
    stored->s_regs[2] = p->trapframe->s2;
    stored->s_regs[3] = p->trapframe->s3;
    stored->s_regs[4] = p->trapframe->s4;
    stored->s_regs[5] = p->trapframe->s5;
    stored->s_regs[6] = p->trapframe->s6;
    stored->s_regs[7] = p->trapframe->s7;
    stored->s_regs[8] = p->trapframe->s8;
    stored->s_regs[9] = p->trapframe->s9;
    stored->s_regs[10] = p->trapframe->s10;
    stored->s_regs[11] = p->trapframe->s11;
    /* t_regs */
    stored->t_regs[0] = p->trapframe->t0;
    stored->t_regs[1] = p->trapframe->t1;
    stored->t_regs[2] = p->trapframe->t2;
    stored->t_regs[3] = p->trapframe->t3;
    stored->t_regs[4] = p->trapframe->t4;
    stored->t_regs[5] = p->trapframe->t5;
    stored->t_regs[6] = p->trapframe->t6;
    /* a_regs */
    stored->a_regs[0] = p->trapframe->a0;
    stored->a_regs[1] = p->trapframe->a1;
    stored->a_regs[2] = p->trapframe->a2;
    stored->a_regs[3] = p->trapframe->a3;
    stored->a_regs[4] = p->trapframe->a4;
    stored->a_regs[5] = p->trapframe->a5;
    stored->a_regs[6] = p->trapframe->a6;
    stored->a_regs[7] = p->trapframe->a7;
  }
  return count;
}

// for mp3
// assume that passed id is llegal,
// and exit = 0 call after switch to handler
// exit = 1 call before switch to handler
uint64
sys_thrdresume(void)
{
  int thrdstop_context_id, is_exit;
  if (argint(0, &thrdstop_context_id) < 0)
    return -1;
  if (argint(1, &is_exit) < 0)
    return -1;

  struct proc *p = myproc();
  if (is_exit == 0)
  {
    struct thrd_context_data *stored = &(p->thrdstop_context[thrdstop_context_id]);
    p->trapframe->ra = stored->ra;
    p->trapframe->gp = stored->gp;
    p->trapframe->sp = stored->sp;
    p->trapframe->tp = stored->tp;
    p->trapframe->epc = stored->epc;
    /* s_regs */
    p->trapframe->s0 = stored->s_regs[0];
    p->trapframe->s1 = stored->s_regs[1];
    p->trapframe->s2 = stored->s_regs[2];
    p->trapframe->s3 = stored->s_regs[3];
    p->trapframe->s4 = stored->s_regs[4];
    p->trapframe->s5 = stored->s_regs[5];
    p->trapframe->s6 = stored->s_regs[6];
    p->trapframe->s7 = stored->s_regs[7];
    p->trapframe->s8 = stored->s_regs[8];
    p->trapframe->s9 = stored->s_regs[9];
    p->trapframe->s10 = stored->s_regs[10];
    p->trapframe->s11 = stored->s_regs[11];
    /* t_regs */
    p->trapframe->t0 = stored->t_regs[0];
    p->trapframe->t1 = stored->t_regs[1];
    p->trapframe->t2 = stored->t_regs[2];
    p->trapframe->t3 = stored->t_regs[3];
    p->trapframe->t4 = stored->t_regs[4];
    p->trapframe->t5 = stored->t_regs[5];
    p->trapframe->t6 = stored->t_regs[6];
    /* a_regs */
    p->trapframe->a0 = stored->a_regs[0];
    p->trapframe->a1 = stored->a_regs[1];
    p->trapframe->a2 = stored->a_regs[2];
    p->trapframe->a3 = stored->a_regs[3];
    p->trapframe->a4 = stored->a_regs[4];
    p->trapframe->a5 = stored->a_regs[5];
    p->trapframe->a6 = stored->a_regs[6];
    p->trapframe->a7 = stored->a_regs[7];
  }
  else  // same as thread_exit
  {
    // bit set 0, thrdstop_context clear, tick and interval clear
    p->thrdstop_context_used[thrdstop_context_id] = 0;
    // if the fact that exit = 1 call when tick < interval is true,
    // then p->thrdstop... attr should also be cleared
    p->thrdstop_handler_pointer = 0;
    p->thrdstop_context_id = -1;
    p->thrdstop_interval = -1;
    p->thrdstop_ticks = 0;
    memset(&(p->thrdstop_context[thrdstop_context_id]), 0, sizeof(struct thrd_context_data));
  }
  return 0;
}
