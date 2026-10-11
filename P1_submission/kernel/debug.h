#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H

// kernel logging. user/dbg.c includes this too.

#define DBG_PROC    (1UL << 0) /* process lifecycle             */
#define DBG_SCHED   (1UL << 1) /* context switching             */
#define DBG_SYSCALL (1UL << 2) /* system call entry/exit        */
#define DBG_TRAP    (1UL << 3) /* traps, interrupts, page faults*/
#define DBG_VM      (1UL << 4) /* virtual memory                */
#define DBG_FS      (1UL << 5) /* file system                   */
#define DBG_DEV     (1UL << 6) /* drivers                       */
#define DBG_ALL     (~0UL)

#define DBG_ERR   1
#define DBG_WARN  2
#define DBG_INFO  3
#define DBG_TRACE 4

#define DBG_LEVEL_MIN 0
#define DBG_LEVEL_MAX DBG_TRACE

#define DBGCTL_GETMASK  0
#define DBGCTL_SETMASK  1
#define DBGCTL_GETLEVEL 2
#define DBGCTL_SETLEVEL 3
#define DBGCTL_GETPID   4
#define DBGCTL_SETPID   5

struct dbgcat {
  uint64 bit;
  char *name;  // what you type
  char *label; // what prints. padded, printk cant do %-7s
};

// unused attr keeps -Werror quiet
static const struct dbgcat dbg_cats[] __attribute__((unused)) = {
  // clang-format off
  { DBG_PROC,    "proc",    "proc   " },
  { DBG_SCHED,   "sched",   "sched  " },
  { DBG_SYSCALL, "syscall", "syscall" },
  { DBG_TRAP,    "trap",    "trap   " },
  { DBG_VM,      "vm",      "vm     " },
  { DBG_FS,      "fs",      "fs     " },
  { DBG_DEV,     "dev",     "dev    " },
  // clang-format on
};

#define DBG_NCATS ((int)(sizeof(dbg_cats) / sizeof(dbg_cats[0])))

static const char *dbg_levelnames[] __attribute__((unused)) = {
  "off", "err", "warn", "info", "trace",
};

#define DBG_NLEVELS ((int)(sizeof(dbg_levelnames) / sizeof(dbg_levelnames[0])))

extern uint64 dbg_mask;
extern int dbg_level;
extern int dbg_pid; // 0 = no filter

char *dbg_catname(uint64 cat);
int dbg_curpid(void);

// macro so the args only run when it prints
#ifndef DBG_DISABLE

static inline int
dbg_test(uint64 cat, int level)
{
  int pid;

  if ((__atomic_load_n(&dbg_mask, __ATOMIC_RELAXED) & (cat)) == 0 ||
      level > __atomic_load_n(&dbg_level, __ATOMIC_RELAXED))
    return 0;

  // pid check last, only costs anything once mask and level pass
  pid = __atomic_load_n(&dbg_pid, __ATOMIC_RELAXED);
  return pid == 0 || pid == dbg_curpid();
}

#define dprintf(cat, level, fmt, ...)                                          \
  do {                                                                         \
    if (dbg_test((cat), (level)))                                              \
      printk("[%s][%d][%s] " fmt "\n", dbg_catname(cat), dbg_curpid(),         \
             __func__, ##__VA_ARGS__);                                         \
  } while (0)

#else

// make DBG=0. if(0) not empty, so logged vars dont come out unused
#define dprintf(cat, level, fmt, ...)                                          \
  do {                                                                         \
    if (0)                                                                     \
      printk(fmt, ##__VA_ARGS__);                                              \
  } while (0)

#endif

#endif
