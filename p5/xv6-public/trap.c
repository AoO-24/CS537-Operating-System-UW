#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "mmap.h"

struct file
{
  enum
  {
    FD_NONE,
    FD_PIPE,
    FD_INODE
  } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);
pte_t *walkpgdir(pde_t *pgdir, const void *va, int alloc);

void tvinit(void)
{
  int i;

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

// Helper function to handle segmentation faults
void handle_segmentation_fault(struct proc *p)
{
  cprintf("Segmentation Fault\n");
  p->killed = 1;
}

void update_vma_structure(struct VMA *next, struct VMA *v_grow, int addr)
{
  v_grow->next = addr;
  next->addr = v_grow->addr + PGSIZE;
  next->end = v_grow->end + PGSIZE;
  next->prot = v_grow->prot;
  next->flags = v_grow->flags;
  next->fd = v_grow->fd;
  next->offset = v_grow->offset + PGSIZE;
  next->valid = 1;
  next->pf = v_grow->pf;
}
// load a page from the disk into the new memory space,
// ensuring the process's memory reflects the contents of the file it's mapped to.
void copy_page_from_disk(struct proc *process, char *memory, struct VMA *next)
{
  struct file *f = process->ofile[next->fd];
  ilock(f->ip);
  readi(f->ip, memory, PGSIZE, PGSIZE); // copy a page of the file from the disk
  iunlock(f->ip);
}

// PAGEBREAK: 41
void trap(struct trapframe *tf)
{
  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0)
    {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT: // Page Fault handler
    struct proc *p = myproc();
    // Fault Address Retrieval
    uint addr = rcr2();
    // Initial Validity Checks:
    if (addr < VMA_START || addr + PGSIZE > VMA_END)
    {
      handle_segmentation_fault(p);
      break;
    }
    // Memory Region Checks:
    if (!is_region_free(p->pgdir, (void *)addr + PGSIZE, PGSIZE))
    {
      handle_segmentation_fault(p);
      break;
    }
    // Growth Direction Check:
    if (is_region_free(p->pgdir, (void *)addr - PGSIZE, PGSIZE))
    {
      cprintf("MAP_GROWSUP: prevs aint free");
      myproc()->killed = 1;
      break;
    }

    uint a = PGROUNDDOWN(addr);
    int next_idx = 0;
    struct VMA *next = 0;
    struct VMA *grow = 0;
    // VMA Expansion Logic:
    // Iterates through the process's virtual memory areas (VMAs) to find
    // the VMA that needs to be expanded to accommodate the new page.
    for (int i = 0; i < 32; i++)
    {
      if (i != 31)
      {
        next = &p->vmas[i + 1];
        next_idx = i + 1;
      }
      // can't growth if page next to gard is used
      if (i < 30 && p->vmas[i + 2].valid == 1)
      {
        grow = 0;
        break;
      }
      struct VMA *cur = &p->vmas[i];
      if ((uint)cur->end <= addr && addr < (uint)cur->end + PGSIZE)
      {
        grow = cur;
        break;
      }
    }
    // If no suitable VMA is found or the VMA does not support upward growth, a segmentation fault is handled.
    if (!grow || !(grow->flags & MAP_GROWSUP))
    {
      handle_segmentation_fault(p);
      break;
    }
    // Allocation and Mapping of New Memory:
    char *mem = kalloc();
    if (mem == 0)
    {
      handle_segmentation_fault(p);
      deallocuvm(p->pgdir, a, a);
      break;
    }
    memset(mem, 0, PGSIZE);
    mappages(p->pgdir, (void *)a, PGSIZE, V2P(mem), PTE_W | PTE_U);

    // Update VMA Structure:
    update_vma_structure(next, grow, next_idx);
    // Handling Non-Anonymous Mappings:
    if (!(next->flags & MAP_ANONYMOUS))
    {
      copy_page_from_disk(p, mem, next);
    }
    break;

  // PAGEBREAK: 13
  default:
    // need to allocate more space
    /*mmap_growsup();*/
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}