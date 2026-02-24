# Lab Agent Context (Temporary)

This file is branch-local guidance for the current lab and is intended to be replaced or removed on other lab branches.

## Current lab

- Read `conf/lab.mk` to confirm active lab.
- This branch is for `LAB=traps` and currently focuses on the alarm exercise (`sigalarm` / `sigreturn`).

## Coaching mode for this branch

- User is implementing the solution.
- Agent should prioritize progress tracking, sanity checks, and correcting misunderstandings.
- Do not proactively implement full solutions unless explicitly asked to code.
- Prefer milestone-based guidance with short verification steps.
- if official guidance is needed as reference, use `exercise-alarm.md` in this directory.

## Alarm exercise checkpoints

1. Syscall plumbing complete (`user/user.h`, `user/usys.pl`, `kernel/syscall.h`, `kernel/syscall.c`, `kernel/sysproc.c`).
2. Per-process alarm state added in `kernel/proc.h` and initialized in `kernel/proc.c`.
3. Timer-interrupt hook in `kernel/trap.c` triggers handler at interval.
4. Trapframe/register state saved and restored via `sigreturn`.
5. Re-entrancy prevented while handler is active.
6. Validation passes: `alarmtest` and `usertests -q`.

## Exit criteria for this overlay

- Once this short-term coaching goal is complete, this file may be deleted or rewritten for the next lab.

## Current progress (2026-02-24 14:20)

- Current intent: follow course sequence and focus on `test0` behavior before implementing full `sigreturn` restore.
- Implemented now:
	- syscall plumbing for `sigalarm`/`sigreturn` is present.
	- per-proc alarm fields (`interval`, `handler`, `ticks`) are present and initialized.
	- timer path in `usertrap()` increments per-proc alarm ticks and redirects `epc` to handler when interval expires.
- Explicitly deferred for later checkpoints:
	- full trapframe/register save+restore in `sigreturn`.
	- re-entrancy guard while handler is active.
- Known semantic gap to revisit: `sigalarm(0, 0)` disable behavior should match exercise text.
