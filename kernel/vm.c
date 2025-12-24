#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

#ifdef LAB_NET
  // PCI-E ECAM (configuration space), for pci.c
  kvmmap(kpgtbl, 0x30000000L, 0x30000000L, 0x10000000, PTE_R | PTE_W);

  // pci.c maps the e1000's registers here.
  kvmmap(kpgtbl, 0x40000000L, 0x40000000L, 0x20000, PTE_R | PTE_W);
#endif  

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// Initialize the kernel_pagetable, shared by all CPUs.
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      // pagetable = (pagetable_t)PTE2PA(*pte); // ? moved down; is that correct?
#ifdef LAB_PGTBL
      if(PTE_LEAF(*pte)) {
        if((va % SUPERPGSIZE) == 0)
          printf("walk: superpage at level %d, va=%p, pte=%p\n", level, (void*)va, pte);
        return pte;
      }
#endif
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}


#if defined(LAB_PGTBL) || defined(SOL_MMAP) || defined(SOL_COW)
/** should print the following output:
  ```
    page table 0x0000000087f22000
     ..0x0000000000000000: pte 0x0000000021fc7801 pa 0x0000000087f1e000
     .. ..0x0000000000000000: pte 0x0000000021fc7401 pa 0x0000000087f1d000
     .. .. ..0x0000000000000000: pte 0x0000000021fc7c5b pa 0x0000000087f1f000
     .. .. ..0x0000000000001000: pte 0x0000000021fc705b pa 0x0000000087f1c000
     .. .. ..0x0000000000002000: pte 0x0000000021fc6cd7 pa 0x0000000087f1b000
     .. .. ..0x0000000000003000: pte 0x0000000021fc6807 pa 0x0000000087f1a000
     .. .. ..0x0000000000004000: pte 0x0000000021fc64d7 pa 0x0000000087f19000
     ..0x0000003fc0000000: pte 0x0000000021fc8401 pa 0x0000000087f21000
     .. ..0x0000003fffe00000: pte 0x0000000021fc8001 pa 0x0000000087f20000
     .. .. ..0x0000003fffffd000: pte 0x0000000021fd4813 pa 0x0000000087f52000
     .. .. ..0x0000003fffffe000: pte 0x0000000021fd00c7 pa 0x0000000087f40000
     .. .. ..0x0000003ffffff000: pte 0x0000000020001c4b pa 0x0000000080007000
  ```
  The first line displays the argument to vmprint. 
  After that there is a line for each PTE, including PTEs that 
    refer to page-table pages deeper in the tree. 
  Each PTE line is indented by a number of " .." that indicates its depth in the tree. 
  Each PTE line shows its virtual addresss, the pte bits, and 
    the physical address extracted from the PTE. 
  Don't print PTEs that are not valid. In the above example, 
    the top-level page-table page has mappings for entries 0 and 255. 
  The next level down for entry 0 has only index 0 mapped, and 
    the bottom-level for that index 0 has a few entries mapped.
  Your code might emit different physical addresses than those shown above. 
    The number of entries and the virtual addresses should be the same.

  Some hints:
  - Use the macros at the end of the file kernel/riscv.h.
  - The function freewalk may be inspirational.
  - Use %p in your printf calls to print out full 64-bit hex PTEs and addresses as shown in the example.
  
> For every leaf page in the vmprint output, explain 
    what it logically contains and what its permission bits are, and 
    how it relates to the output of the earlier print_pgtbl() exercise above. 
  Figure 3.4 in the xv6 book might be helpful, although note that 
    the figure might have a slightly different set of pages than the process that's being inspected here.
*/
void
vmprint(pagetable_t pagetable) {
  // your code here
  printf("page table %p\n", pagetable);
  void vmprint_helper(pagetable_t pagetable, int level, uint64 va_prefix);
  vmprint_helper(pagetable, 0, 0);
}

void
vmprint_helper(pagetable_t pagetable, int level, uint64 va_prefix) {
  for(int i = 0; i < 512; i++){
    pte_t *pte = &pagetable[i];
    if((*pte & PTE_V) == 0)  // has physical page been allocated?
      continue;

    uint64 va = va_prefix | ((uint64)i << PXSHIFT(2 - level));
    for(int j = 0; j <= level; j++) {
      printf(" ..");
    }
    // printf("0x%lx: pte 0x%lx pa 0x%lx\n", va, *pte, PTE2PA(*pte));
    printf("%p: pte %p pa %p\n", (void*) va, (void*) *pte, (void*) PTE2PA(*pte));

    if(!PTE_LEAF(*pte)) {
      pagetable_t next_level = (pagetable_t)PTE2PA(*pte);
      vmprint_helper(next_level, level + 1, va);
    }
  }
}
#endif



// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - PGSIZE;
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

#ifdef LAB_PGTBL
// Walk to level-1 PTE (for superpage mapping).
// Returns pointer to level-1 PTE, or 0 if allocation fails.
static pte_t *
walk_level1(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk_level1");

  // Level 2 → Level 1
  pte_t *pte = &pagetable[PX(2, va)];
  if(*pte & PTE_V) {
    pagetable = (pagetable_t)PTE2PA(*pte);
  } else {
    if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
      return 0;
    memset(pagetable, 0, PGSIZE);
    *pte = PA2PTE(pagetable) | PTE_V;
  }
  // Return level-1 PTE
  return &pagetable[PX(1, va)];
}

int
mapsuperpages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % SUPERPGSIZE) != 0)
    panic("mapsuperpages: va not aligned");

  if((size % SUPERPGSIZE) != 0)
    panic("mapsuperpages: size not aligned");

  if(size == 0)
    panic("mapsuperpages: size");
  
  a = va;
  last = va + size - SUPERPGSIZE;
  for(;;){
    if((pte = walk_level1(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mapsuperpages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += SUPERPGSIZE;
    pa += SUPERPGSIZE;
  }
  return 0;
}
#endif // LAB_PGTBL

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

#ifdef LAB_PGTBL
// Demote a superpage mapping at va to 512 regular page mappings.
// Remaps the same physical pages in-place (no allocation or copying).
// va must be superpage-aligned.
// Returns 0 on success, -1 if not a superpage.
static int
uvmdemote(pagetable_t pagetable, uint64 va)
{
  if((va % SUPERPGSIZE) != 0)
    return -1;

  pte_t *pte1 = walk_level1(pagetable, va, 0);
  if(pte1 == 0 || (*pte1 & PTE_V) == 0 || !PTE_LEAF(*pte1))
    return -1;  // not a superpage

  uint64 pa = PTE2PA(*pte1);
  uint flags = PTE_FLAGS(*pte1);

  // Clear superpage PTE first
  *pte1 = 0;

  // Create level-0 mappings to the same physical pages
  for(int i = 0; i < 512; i++){
    if(mappages(pagetable, va + i*PGSIZE, PGSIZE, pa + i*PGSIZE, flags) != 0){
      panic("uvmdemote: mappages failed");
    }
  }

  return 0;
}

// Unmap a full superpage at va.
// va must be superpage-aligned and point to a superpage.
// Returns 1 if superpage was unmapped, 0 if not a superpage.
// Note: Unlike uvmunmap, we don't panic when PTE_V is set without R/W/X bits.
// At level-1, that's valid — it means the PTE points to a level-0 page table
// (i.e., regular 4KB pages exist here, not a superpage). We just return 0.
static int
uvmsuperunmap(pagetable_t pagetable, uint64 va, int do_free)
{
  if((va % SUPERPGSIZE) != 0)
    panic("uvmsuperunmap: va not aligned");

  pte_t *pte = walk_level1(pagetable, va, 0);
  if(pte == 0 || (*pte & PTE_V) == 0 || !PTE_LEAF(*pte))
    return 0;  // not a superpage

  if(do_free){
    uint64 pa = PTE2PA(*pte);
    superfree((void*)pa);
  }
  *pte = 0;
  return 1;
}
#endif

// Remove npages of mappings starting from va. va must be
// page-aligned. It's OK if the mappings don't exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;
  uint64 sz;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += sz){
    sz = PGSIZE;  // default increment

#ifdef LAB_PGTBL
    // Try full superpage unmap first
    if((a % SUPERPGSIZE) == 0 && uvmsuperunmap(pagetable, a, do_free)){
      sz = SUPERPGSIZE;
      continue;
    }

    // Check if we're inside a superpage that needs demotion.
    // We use walk_level1 directly to check the level-1 PTE,
    // because walk() returns a leaf PTE for both superpages and 4KB pages
    // and we can't distinguish them from the PTE alone.
    if((a % SUPERPGSIZE) != 0) {
      uint64 super_va = SUPERPGROUNDDOWN(a);
      pte_t *pte1 = walk_level1(pagetable, super_va, 0);
      if(pte1 != 0 && (*pte1 & PTE_V) && PTE_LEAF(*pte1)){
        // We're inside a superpage — demote it first
        if(uvmdemote(pagetable, super_va) < 0)
          panic("uvmunmap: demotion failed");
      }
    }
#endif

    pte = walk(pagetable, a, 0);

    if(pte == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");

    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}


// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) // ANCHOR[id=uvmalloc] uvmalloc
{
  char *mem;
  uint64 a;
#ifndef LAB_PGTBL
  int sz; // ? why is there such a variable?
#endif

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);

#ifndef LAB_PGTBL
  // Original non-superpage path
  for(a = oldsz; a < newsz; a += sz){
    sz = PGSIZE;
    mem = kalloc(); // LINK kernel/kalloc.c#kalloc
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
#ifndef LAB_SYSCALL
    memset(mem, 0, sz);
#endif
    if(mappages(pagetable, a, sz, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
#else // * keep kalloc for 4K, and use superalloc for 2MB allocations.
  // Phase 1: 4KB pages until superpage-aligned
  uint64 super_start = SUPERPGROUNDUP(oldsz);
  for(a = oldsz; a < newsz && a < super_start; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_W|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }

  // Phase 2: superpages for aligned 2MB chunks
  for(; a + SUPERPGSIZE <= newsz; a += SUPERPGSIZE){
    mem = superalloc();
    printf("uvmalloc: superalloc at va=%p returned pa=%p\n", (void*)a, mem);
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, SUPERPGSIZE);
    if(mapsuperpages(pagetable, a, SUPERPGSIZE, (uint64)mem, PTE_R|PTE_W|PTE_U|xperm) != 0){
      superfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }

  // Phase 3: 4KB pages for remainder (not tested but good for completeness)
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_W|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
#endif
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      // backtrace();
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; ){
#ifdef LAB_PGTBL
    // Check if this is a superpage
    pte_t *pte1 = walk_level1(old, i, 0);
    if(pte1 != 0 && (*pte1 & PTE_V) && PTE_LEAF(*pte1)){
      // It's a superpage - copy the whole 2MB
      pa = PTE2PA(*pte1);
      flags = PTE_FLAGS(*pte1);
      if((mem = superalloc()) == 0)
        goto err;
      memmove(mem, (char*)pa, SUPERPGSIZE);
      if(mapsuperpages(new, i, SUPERPGSIZE, (uint64)mem, flags) != 0){
        superfree(mem);
        goto err;
      }
      i += SUPERPGSIZE;
      continue;
    }
#endif
    // Regular 4KB page
    if((pte = walk(old, i, 0)) == 0){
      i += PGSIZE;
      continue;
    }
    if((*pte & PTE_V) == 0) {
      i += PGSIZE;
      continue;
    }
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
    i += PGSIZE;
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if (va0 >= MAXVA)
      return -1;

    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }

    if((pte = walk(pagetable, va0, 0)) == 0) {
      // printf("copyout: pte should exist %lx %ld\n", dstva, len);
      return -1;
    }


    // forbid copyout over read-only user text pages.
    if((*pte & PTE_W) == 0)
      return -1;
    
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;
  
  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}




// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();
  

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if(ismapped(pagetable, va)) {
    return 0;
  }
  mem = (uint64) kalloc();
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }
  return mem;
}

int
ismapped(pagetable_t pagetable, uint64 va) {
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0) {
    return 0;
  }
  if (*pte & PTE_V){
    return 1;
  }
  return 0;
}



#ifdef LAB_PGTBL
pte_t*
pgpte(pagetable_t pagetable, uint64 va) {
  return walk(pagetable, va, 0);
}
#endif
