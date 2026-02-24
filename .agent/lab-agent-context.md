# Lab Agent Context — COW (branch-local)

This file provides temporary, branch-local guidance for the MIT 6.1810 xv6 **Copy-on-Write (COW) fork** lab.

Your job is to *help* and *guide* the student through the lab; you may write code when asked, but follow the roadmap step by step and do not work on a problem that comes after what the student is currently focussing on. 
Use this context to understand the lab requirements and constraints, so you can provide accurate and helpful advice when the student asks questions about implementation details, debugging, or testing.

## Source of truth
- Lab page: https://pdos.csail.mit.edu/6.828/2025/labs/cow.html
- Completion target: pass both `cowtest` and `usertests -q`.

## Objective
Implement COW `fork()` so xv6 defers copying user pages until a write actually occurs.

## Required behavior
1. `fork()` shares physical user pages between parent and child at first.
2. Shared COW pages are mapped read-only in **both** parent and child.
3. On write fault to a COW page:
   - allocate a new page,
   - copy old content,
   - remap faulting process PTE as writable (private copy),
   - keep the other process mapping unchanged.
4. Originally read-only pages (e.g., text) must stay read-only/shared; attempted writes should kill the process.
5. Physical pages are freed **only** when their last mapping disappears (reference counting).
6. `copyout()` must handle COW pages with the same COW break logic as user page faults.

## Suggested implementation map
- `kernel/vm.c`
  - Update `uvmcopy()` to share pages rather than copy them.
  - Clear `PTE_W` and mark COW on formerly writable mappings.
  - Implement/adjust COW fault handling path (commonly `vmfault()` in this lab setup).
  - Update `copyout()` to trigger COW break when needed.
- `kernel/trap.c`
  - Route store page faults to COW handler.
  - Kill process on invalid/non-COW write faults or OOM during COW allocation.
- `kernel/kalloc.c`
  - Add/maintain per-physical-page refcount array.
  - `kalloc()`: initialize refcount for newly allocated page.
  - Shared mapping creation: increment refcount.
  - Unmapping/free path: decrement and only free when count reaches zero.
- `kernel/riscv.h`
  - Use RSW bits to encode a COW marker flag in PTEs (lab hint).

## Invariants to keep
- Refcount changes must be synchronized correctly (lock discipline in allocator paths).
- Never grant write access to a shared COW page without first creating a private copy.
- On COW break, clear COW marker and set `PTE_W` only for the faulting process’s new mapping.
- Preserve xv6 error handling style: if no memory on COW fault, kill offending process.

## Validation workflow
1. Build and run quick check:
   - `make qemu`
   - from xv6 shell: `cowtest`
2. Full lab validation:
   - from xv6 shell: `usertests -q`
3. Grading script:
   - `make grade`
You will not be able to see inside the vm shell; ask the human to do that.

## Submission reminders from lab page
- Add `time.txt` containing hours spent.
- Commit tracked changes before `make zipball`.
- Upload generated `lab.zip` to Gradescope.

## Non-goals
- Do not add features outside COW requirements.
- Keep edits minimal and lab-focused; avoid cross-lab coupling.
