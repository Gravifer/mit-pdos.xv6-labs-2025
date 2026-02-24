# Lab traps Agent Context

**Date**: 2026-02-24  
**Status**: Core lab COMPLETE ✅ | Optional challenge NOT STARTED

---

## Summary

The required `sigalarm`/`sigreturn` alarm handler exercise is fully implemented and all tests pass:
- ✅ test0: handler invoked and returns correctly
- ✅ test1: registers preserved across handler
- ✅ test2: re-entrancy protection prevents nested invocations
- ✅ test3: a0 specifically preserved

## Implementation Overview

### Key Components

**`struct alarm` (proc.h)**
```c
struct alarm {
  int interval;                        // ticks between alarms; -1 = inactive
  void (*handler)(void);               // handler function pointer
  uint ticks;                          // counter (unsigned for safe overflow)
  struct trapframe *saved_trapframe;   // allocated trapframe copy
  int in_handler;                      // re-entrancy guard
}
```

**Timer interrupt flow (trap.c)**
1. Every timer tick, increment alarm.ticks
2. When ticks >= interval: save full trapframe to saved_trapframe, set epc = handler, set in_handler = 1
3. Re-entrancy guard prevents nested invocations

**Handler return (sysproc.c → syscall.c)**
1. sys_sigreturn() restores entire trapframe from saved_trapframe
2. Special handling in syscall() dispatcher: skip `a0 = return_value` for SYS_sigreturn (preserve restored a0)

### Files Modified
- `kernel/proc.h`: Added alarm struct
- `kernel/proc.c`: Initialize alarm; kfree saved_trapframe on exit
- `kernel/trap.c`: Timer tick handler with reentrant check
- `kernel/sysproc.c`: sys_sigalarm and sys_sigreturn implementations
- `kernel/syscall.c`: Special case for sigreturn a0 preservation

### Design Decisions

**Why -1 for "inactive"?** Allows signed arithmetic; interval <= 0 all treated as inactive.

**Why separate saved_trapframe?** Can't modify p->trapframe in-place; need two separate 4KB buffers (main trapframe + saved snapshot).

**Why special handling in syscall()?** The dispatcher unconditionally assigns return value to a0. For sigreturn, we already restored a0 from saved state, so we must skip the dispatcher's assignment.

**Why unsigned ticks?** Safe overflow semantics with >= comparison when interval is positive.

---

## Optional Challenge: Function Names in Backtrace

**Current backtrace()** (kernel/printf.c, line ~161) prints raw addresses. The optional challenge asks to print function names and line numbers instead.

**Entry point**: `kernel/printf.c` backtrace() function

**Required approach**:
1. Extract kernel symbols from `kernel/kernel.sym` (266 lines, manageable size)
2. Embed static symbol table in kernel
3. Create address-to-symbol lookup function
4. Modify backtrace() printf to show names

**Test**: Run `bttest` → should see file names (sysproc.c, syscall.c, trap.c) in output

**For next agent**: Symbol table generation can be done via Makefile rule or build-time script.

---

## Verification

Run full suite:
```bash
make qemu
alarmtest      # should show: test0 passed, test1 passed, test2 passed, test3 passed
usertests -q   # should pass without new failures
./grade-lab-traps  # core tests should all pass
```

---

## State of the Code

All required functionality is in place and working. The code is clean, well-commented, and ready for submission. No known bugs or regressions.
