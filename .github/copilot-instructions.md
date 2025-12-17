# xv6 RISC-V Copilot Instructions

This is **xv6**, a teaching operating system for MIT 6.1810. It's a re-implementation of Unix V6 for RISC-V multiprocessors.

## Architecture Overview

### Directory Structure
- `kernel/` — Kernel source (runs in supervisor mode)
- `user/` — User programs and library (runs in user mode)
- `mkfs/` — Filesystem image builder (runs on host)

### Key Kernel Components
| File | Purpose |
|------|---------|
| `proc.c` | Process management, scheduler, fork/exit/wait |
| `vm.c` | Virtual memory, page tables (Sv39 3-level) |
| `kalloc.c` | Physical page allocator (4KB pages) |
| `trap.c` | Interrupt/exception handling, syscall dispatch |
| `syscall.c` | System call argument parsing |
| `sysproc.c` / `sysfile.c` | System call implementations |

### Memory Layout (see `memlayout.h`)
- `KERNBASE` (0x80000000): Kernel starts here
- `PHYSTOP`: End of physical RAM (128MB)
- `TRAMPOLINE`: Highest virtual address (shared kernel/user)
- `TRAPFRAME`: Per-process, just below trampoline

## Build & Run Commands

```bash
make qemu          # Build and run xv6 in QEMU (creates fresh fs.img)
make qemu-fs       # Run with existing fs.img
make qemu-gdb      # Run with GDB server (use `gdb` in another terminal)
make clean         # Remove build artifacts
make grade         # Run lab grading script
```

Current lab is set in `conf/lab.mk` (e.g., `LAB=pgtbl`). This enables lab-specific code via `#ifdef LAB_PGTBL`.

## Adding a System Call

1. Add number in `kernel/syscall.h`: `#define SYS_mysyscall N`
2. Add entry in `user/usys.pl`: `entry("mysyscall");`
3. Declare in `user/user.h`: `int mysyscall(args);`
4. Implement in `kernel/sysproc.c` or `kernel/sysfile.c`:
   ```c
   uint64 sys_mysyscall(void) {
     int arg;
     argint(0, &arg);  // fetch arg from a0
     // ... implementation
     return result;
   }
   ```
5. Add to dispatch table in `kernel/syscall.c`

## Code Conventions

### Locking Pattern
Always use `acquire()`/`release()` pairs. Interrupts are disabled while holding spinlocks:
```c
acquire(&kmem.lock);
// critical section
release(&kmem.lock);
```

### Memory Allocation
- `kalloc()` returns a 4KB page (zeroed with junk pattern 5)
- `kfree()` takes physical address, fills with junk pattern 1
- Always check for NULL return from `kalloc()`

### Page Table Operations
- `walk(pagetable, va, alloc)` — Find/create PTE for virtual address
- `mappages()` — Map range of virtual addresses
- `uvmalloc()`/`uvmdealloc()` — Grow/shrink user memory
- Use `PTE_R`, `PTE_W`, `PTE_X`, `PTE_U` flags from `riscv.h`

### Process Access
- `myproc()` — Get current process (interrupts must be enabled)
- `p->trapframe->a0..a5` — Syscall arguments
- `p->pagetable` — User page table

## Lab System

Labs are configured in `conf/lab.mk` (e.g., `LAB=pgtbl`). This enables:
- Conditional compilation via `#ifdef LAB_PGTBL`, `#ifdef LAB_COW`, etc.
- Lab-specific user programs in the Makefile (e.g., `_pgtbltest`, `_cowtest`)
- Lab-specific grading via `./grade-lab-<name>`

Common labs: `util`, `syscall`, `pgtbl`, `traps`, `cow`, `thread`, `lock`, `fs`, `mmap`, `net`

When implementing lab features, wrap lab-specific code in `#ifdef LAB_<NAME>` blocks to maintain compatibility.

## Debugging Tips

- `printf()` works in kernel (use for debugging)
- `panic("message")` halts with message
- Generated `.asm` files show disassembly (e.g., `kernel/kernel.asm`)
- Use `vmprint()` to dump page tables
- QEMU with GDB: `make qemu-gdb`, then `riscv64-linux-gnu-gdb`
