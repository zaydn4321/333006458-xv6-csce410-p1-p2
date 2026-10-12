// harder scheduler tests: round robin, all levels, kill wakeup, churn.
// run with CPUS=1, rq_check is only a clean snapshot on one cpu

#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/debug.h"
#include "user/user.h"

#define LOW (NB_PRIORITY_LEVELS - 1)

static int fails = 0;
static struct schedev ev[SCHEDLOG_N];

static void
check(int ok, char *what)
{
  printf("schedtest: %s %s\n", what, ok ? "OK" : "FAIL");
  if (!ok)
    fails++;
}

// burn about n ticks of our own cpu time. every tick preempts us, so each
// time uptime changes we just finished a slice, however long others ran.
// tick based so it doesnt depend on how fast the machine is
static void
work(int n)
{
  int last = uptime();

  while (n > 0) {
    int t = uptime();
    if (t != last)
      n--;
    last = t;
  }
}

// 3 cpu bound kids at the same level. with round robin they all get
// turns before any of them finishes, fifo-to-completion would run them
// one after another
static void
test_rr(void)
{
  int pids[3], cnt[3] = { 0 }, first[3], last[3];

  setpriority(0);
  debugctl(DBGCTL_SCHEDLOG, (uint64)ev); // clear it
  for (int i = 0; i < 3; i++) {
    if ((pids[i] = fork()) == 0) {
      setpriority(LOW);
      work(5);
      exit(0);
    }
  }
  for (int i = 0; i < 3; i++)
    wait(0);
  setpriority(DEFAULT_PRIORITY);

  int n = debugctl(DBGCTL_SCHEDLOG, (uint64)ev);
  check(n <= SCHEDLOG_N, "trace didnt overflow");
  for (int i = 0; i < n && i < SCHEDLOG_N; i++)
    for (int c = 0; c < 3; c++)
      if (ev[i].pid == pids[c] && ev[i].prio == LOW) {
        if (cnt[c]++ == 0)
          first[c] = i;
        last[c] = i;
      }

  int ok = 1, maxfirst = 0, minlast = SCHEDLOG_N;
  for (int c = 0; c < 3; c++) {
    printf("schedtest: pid %d ran %d times at prio %d\n", pids[c], cnt[c], LOW);
    if (cnt[c] < 2) {
      ok = 0;
      continue;
    }
    if (first[c] > maxfirst)
      maxfirst = first[c];
    if (last[c] < minlast)
      minlast = last[c];
  }
  check(ok && maxfirst < minlast, "same prio kids take turns (round robin)");
}

// one kid per level, forked lowest prio first. they should finish 0,1,2,..
// starvation turns are too rare to change the order
static void
test_levels(void)
{
  int fds[2];
  char got[NB_PRIORITY_LEVELS + 1] = { 0 }, want[NB_PRIORITY_LEVELS + 1] = { 0 };

  pipe(fds);
  setpriority(0);
  for (int i = NB_PRIORITY_LEVELS - 1; i >= 0; i--) {
    want[i] = '0' + i;
    if (fork() == 0) {
      close(fds[0]);
      setpriority(i);
      work(3);
      write(fds[1], &want[i], 1);
      exit(0);
    }
  }
  close(fds[1]);
  for (int i = 0; i < NB_PRIORITY_LEVELS; i++)
    read(fds[0], &got[i], 1);
  close(fds[0]);
  for (int i = 0; i < NB_PRIORITY_LEVELS; i++)
    wait(0);
  setpriority(DEFAULT_PRIORITY);
  printf("schedtest: finish order %s\n", got);
  check(strcmp(got, want) == 0, "every level finishes in priority order");
}

// kill wakes a sleeping lowest prio kid while a prio 1 hog spins.
// kill has to put it in the right queue and starvation prevention has
// to let it exit before the hog is done
static void
test_killwake(void)
{
  setpriority(0);
  int sleeper = fork();
  if (sleeper == 0) {
    setpriority(NB_PRIORITY_LEVELS - 1);
    pause(1000);
    exit(0);
  }
  pause(2); // let it get to sleep
  int hog = fork();
  if (hog == 0) {
    setpriority(1);
    int end = uptime() + 5 * STARVE_TICKS;
    while (uptime() < end)
      ;
    exit(0);
  }
  kill(sleeper);
  int bad = debugctl(DBGCTL_SCHEDCHK, 0);
  int firstdone = wait(0);
  wait(0);
  setpriority(DEFAULT_PRIORITY);
  check(bad == 0, "rq_check clean right after kill wakeup");
  check(firstdone == sleeper, "killed low prio sleeper exits while hog spins");
}

// 8 kids keep changing priority, sleeping and forking while the parent
// runs rq_check every tick. kids also check sleep and fork keep priority
static void
test_churn(void)
{
  enum { NKIDS = 8, ROUNDS = 20 };
  int bad = 0, failed = 0, st;

  setpriority(0);
  for (int k = 0; k < NKIDS; k++) {
    if (fork() == 0) {
      int wrong = 0;
      for (int r = 0; r < ROUNDS; r++) {
        int pr = (k + r) % NB_PRIORITY_LEVELS;
        setpriority(pr);
        pause(1);
        if (getpriority() != pr)
          wrong = 1;
        if (r == ROUNDS / 2) {
          if (fork() == 0)
            exit(getpriority());
          wait(&st);
          if (st != pr)
            wrong = 1;
        }
      }
      exit(wrong);
    }
  }
  for (int i = 0; i < 30; i++) {
    bad += debugctl(DBGCTL_SCHEDCHK, 0);
    pause(1);
  }
  for (int k = 0; k < NKIDS; k++) {
    wait(&st);
    if (st != 0)
      failed++;
  }
  setpriority(DEFAULT_PRIORITY);
  check(bad == 0, "rq_check clean while 8 kids churn");
  check(failed == 0, "kids keep their priority across sleep and fork");
}

int
main(void)
{
  test_rr();
  test_levels();
  test_killwake();
  test_churn();
  check(debugctl(DBGCTL_SCHEDCHK, 0) == 0, "rq_check clean at end");
  printf(fails ? "schedtest: %d FAILED\n" : "schedtest: all OK\n", fails);
  exit(fails != 0);
}
