Transcripts for P1. Each file is a full console session: boot, the
commands typed at the xv6 shell (after each "$ "), and their output.
Every session starts from a freshly built fs.img.

1_boot_silent.txt         Scenario 1. Boot with mask 0: dbg shows mask 0,
                          echo and ls print no debug output.

2_proc_ls_echo.txt        Scenario 2. dbg on proc, then ls and echo hi.
                          Each command shows kfork (shell pid 2 -> child),
                          kexec, kexit, and kwait with matching pids.

3_forktest.txt            Scenario 3. dbg on proc at level trace, then
                          forktest. Shows allocproc/freeproc for every
                          process as well as fork/exit/wait.
3_forktest_check.txt      Check over 3_forktest.txt: every forked pid
                          exited, was reaped, and was freed exactly once.
                          No leaks, no double frees.

4_off_all.txt             Scenario 4. ls, echo hi, forktest with proc on
                          (4, 4 and 189 debug lines), then dbg off all and
                          the same three commands again: 0 debug lines.
                          The 2 lines under "dbg off all" are the dbg
                          process itself, forked before the mask changed.

5_sched.txt               Scenario 5. dbg on sched alone prints nothing,
                          because the sched message is at trace and the
                          default level is info. After dbg level trace,
                          echo hi gives 3 lines and forktest gives 106
                          sched lines around its 2 lines of real output.

6_usertests.txt           Scenario 6. usertests with mask 0: ALL TESTS
                          PASSED, and no debug lines in the output.

7_bonus_pid_filter.txt    Bonus (filter by pid). dbg pid 2 shows only the
                          shell's own kfork/kwait for echo hi, the child's
                          exec/exit are hidden. dbg pid off brings them
                          back. Bad pid input is rejected.

8_extra_errors_levels.txt Extra coverage: kill of a missing pid (WARN),
                          exec of a missing program (ERR), and the level
                          threshold: at warn the INFO messages disappear,
                          at err only the ERR one is left. Also malformed
                          dbg input (unknown category/level, missing args,
                          unknown verb) handled without a kernel panic.
