#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "psched.h"

int sys_fork(void)
{
  return fork();
}

int sys_exit(void)
{
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
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

int sys_sleep(void)
{
  int n;
  uint initial_ticks;

  // Retrieve the argument and check its validity.
  if (argint(0, &n) < 0)
    return -1;

  // Acquire the ticks lock.
  acquire(&tickslock);

  // Record the current tick count.
  initial_ticks = ticks;

  // Calculate and set the wakeup time for the process.
  struct proc *current_process = myproc();
  current_process->wakeuptime = initial_ticks + n;

  // Sleep until the specified wakeup time is reached.
  while (ticks < current_process->wakeuptime)
  {
    // If the process is killed, release the lock and exit.
    if (current_process->killed)
    {
      release(&tickslock);
      return -1;
    }

    // Put the process to sleep and wait for a wakeup signal.
    sleep(&ticks, &tickslock);
  }

  // Release the ticks lock.
  release(&tickslock);

  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int sys_nice(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;

  if (n < 0 || n > 20)
    return -1;

  acquire(&tickslock);

  struct proc *curproc = myproc();
  int prev_nice_value = curproc->nice_value;

  curproc->nice_value = n;

  release(&tickslock); // Unlock process table

  return prev_nice_value;
}

int sys_getschedstate(void)
{
  struct pschedinfo *psi;
  if (argptr(0, (char **)&psi, sizeof(struct pschedinfo)) < 0)
    return -1;
  if (psi == 0) // Check for NULL pointer
    return -1;
  return getschedstate(psi);
}
