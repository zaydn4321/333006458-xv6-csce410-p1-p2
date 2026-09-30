// simple tests for setpriority/getpriority and the priority scheduler

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/debug.h"
#include "user/user.h"

static int fails = 0;

static void
check(int ok, char *what)
{
  printf("prtest: %s %s\n", what, ok ? "OK" : "FAIL");
  if (!ok)
    fails++;
}

// volatile so the loop doesnt get optimized out, returned so -Werror is happy
static int
spin(int n)
{
  volatile int x = 0;
  for (int i = 0; i < n; i++)
    x++;
  return x;
}

static void
test_default(void)
{
  check(getpriority() == DEFAULT_PRIORITY, "default priority is 1");
}

static void
test_invalid(void)
{
  int ok = setpriority(-1) == -1 && setpriority(NB_PRIORITY_LEVELS) == -1 &&
           setpriority(1000) == -1 && getpriority() == DEFAULT_PRIORITY;
  check(ok, "bad values return -1, priority unchanged");
}

static void
test_valid(void)
{
  int ok = 1;
  for (int i = 0; i < NB_PRIORITY_LEVELS; i++)
    if (setpriority(i) != 0 || getpriority() != i)
      ok = 0;
  ok = ok && setpriority(DEFAULT_PRIORITY) == 0;
  ok = ok && setpriority(DEFAULT_PRIORITY) == 0; // same value
  check(ok, "every valid level can be set");
}

static void
test_inherit(void)
{
  int st = -1;

  setpriority(2);
  int pid = fork();
  if (pid == 0)
    exit(getpriority());
  wait(&st);
  setpriority(DEFAULT_PRIORITY);
  check(pid > 0 && st == 2, "fork child inherits priority");
}

// low forks first so plain round robin would let it go first.
// with priorities the prio 0 child should finish before the prio 2 one
static void
test_order(void)
{
  int fds[2];
  char buf[3] = { 0 };

  pipe(fds);
  setpriority(0); // so the parent keeps running until it waits
  for (int i = 0; i < 2; i++) {
    int pid = fork();
    if (pid == 0) {
      close(fds[0]);
      setpriority(i == 0 ? 2 : 0);
      spin(50000000);
      write(fds[1], i == 0 ? "L" : "H", 1);
      exit(0);
    }
  }
  close(fds[1]);
  int bad = debugctl(DBGCTL_SCHEDCHK, 0);
  read(fds[0], buf, 1);
  read(fds[0], buf + 1, 1);
  close(fds[0]);
  wait(0);
  wait(0);
  setpriority(DEFAULT_PRIORITY);
  printf("prtest: finish order %s\n", buf);
  check(bad == 0, "rq_check clean with children queued");
  check(buf[0] == 'H' && buf[1] == 'L', "prio 0 finishes before prio 2");
}

int
main(void)
{
  test_default();
  test_invalid();
  test_valid();
  test_inherit();
  test_order();
  check(debugctl(DBGCTL_SCHEDCHK, 0) == 0, "rq_check clean at end");
  printf(fails ? "prtest: %d FAILED\n" : "prtest: all OK\n", fails);
  exit(fails != 0);
}
