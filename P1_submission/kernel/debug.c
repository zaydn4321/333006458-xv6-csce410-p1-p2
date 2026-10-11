#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "debug.h"

// no lock, reads cant tear
uint64 dbg_mask = 0;
int dbg_level = DBG_INFO;
int dbg_pid = 0;

char *
dbg_catname(uint64 cat)
{
  for (int i = 0; i < DBG_NCATS; i++)
    if (dbg_cats[i].bit & cat)
      return dbg_cats[i].label;
  return "?      ";
}

// -1 if no process
int
dbg_curpid(void)
{
  struct proc *p = myproc();

  return p ? p->pid : -1;
}

// returns the old value, or -1 on a bad op/level
uint64
sys_debugctl(void)
{
  int op;
  uint64 arg, old;

  argint(0, &op);
  argaddr(1, &arg);

  switch (op) {
  case DBGCTL_GETMASK:
    return __atomic_load_n(&dbg_mask, __ATOMIC_RELAXED);

  case DBGCTL_SETMASK:
    old = __atomic_load_n(&dbg_mask, __ATOMIC_RELAXED);
    __atomic_store_n(&dbg_mask, arg, __ATOMIC_RELAXED);
    return old;

  case DBGCTL_GETLEVEL:
    return __atomic_load_n(&dbg_level, __ATOMIC_RELAXED);

  case DBGCTL_SETLEVEL:
    if (arg > DBG_LEVEL_MAX) // arg is unsigned, so this catches negatives too
      return -1;
    old = __atomic_load_n(&dbg_level, __ATOMIC_RELAXED);
    __atomic_store_n(&dbg_level, (int)arg, __ATOMIC_RELAXED);
    return old;

  case DBGCTL_GETPID:
    return __atomic_load_n(&dbg_pid, __ATOMIC_RELAXED);

  case DBGCTL_SETPID:
    if (arg > 0x7fffffff) // has to fit in an int pid
      return -1;
    old = __atomic_load_n(&dbg_pid, __ATOMIC_RELAXED);
    __atomic_store_n(&dbg_pid, (int)arg, __ATOMIC_RELAXED);
    return old;
  }

  return -1;
}
