// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#ifdef LAB_PGTBL
// TODO: megapage - 2MB super pages.
#define NSUPERPAGES 8
#endif

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

// Forward declarations for internal functions
static void kfree_internal(void *pa, int is_locked, int no_memset);
static void *kalloc_internal(int is_locked, int no_memset);
#ifdef LAB_PGTBL
static void superdemote_internal(void *pa, int is_locked, int no_memset);
#endif

struct {
  struct spinlock lock;
  struct run *freelist;
  struct run *superfreelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
#ifdef LAB_PGTBL
  // reserve superpages at the top of physical memory
  char *pa = (char *)(PHYSTOP - NSUPERPAGES * SUPERPGSIZE);
  superfreerange(pa, (void*)PHYSTOP);
  freerange(end, pa);
#else
  freerange(end, (void*)PHYSTOP);
#endif
}

void
freerange(void *pa_start, void *pa_end) // ANCHOR[id=freerange] freerange
{ // TODO: handle superpages
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  kfree_internal(pa, 0, 0);
}

static void
kfree_internal(void *pa, int is_locked, int no_memset)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  if(!no_memset)
    memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  if(!is_locked)
    acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(!is_locked)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void) // ANCHOR[id=kalloc] kalloc
{
  return kalloc_internal(0, 0);
}
static void *
kalloc_internal(int is_locked, int no_memset) // ANCHOR[id=kalloc] kalloc
{
  struct run *r;

  if(!is_locked)
    acquire(&kmem.lock);

  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
  }
#ifdef LAB_PGTBL
  else if(kmem.superfreelist) {
    // Demote a superpage into 512 regular pages
    struct run *super = kmem.superfreelist;
    kmem.superfreelist = super->next;
    superdemote_internal(super, 1, 0);
    r = kmem.freelist;
    if(r)
      kmem.freelist = r->next;
  }
#endif

  if(!is_locked)
    release(&kmem.lock);

  if(r && !no_memset)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

#ifdef LAB_PGTBL
void superfreerange(void *pa_start, void *pa_end)
{ // LINK #freerange
  char *p;
  p = (char*)SUPERPGROUNDUP((uint64)pa_start);
  for(; p + SUPERPGSIZE <= (char*)pa_end; p += SUPERPGSIZE)
    superfree(p);
}

void
superfree(void *pa)
{ // LINK #kfree
  // push to superfreelist
  struct run *r;

  if(((uint64)pa % SUPERPGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("superfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 2, SUPERPGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.superfreelist;
  kmem.superfreelist = r;
  release(&kmem.lock);
  return;
}

void *
superalloc(void)
{ // LINK #kalloc
  // pop from superfreelist
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.superfreelist;
  if(r)
    kmem.superfreelist = r->next;
  release(&kmem.lock);
  if(r)
    memset((char*)r, 5, SUPERPGSIZE); // fill with junk
  return (void*)r;
}

// demote a superpage at pa into normal pages, preserving content
void
superdemote(void *pa)
{
  superdemote_internal(pa, 0, 1);  // no_memset=1 to preserve content
}
static void
superdemote_internal(void *pa, int is_locked, int no_memset)
{
  // Split superpage into 512 regular pages
  char *p = (char*)pa;
  for(int i = 0; i < 512; i++){
    // Note: no_memset=1 to preserve content for partial-free demotion; for kalloc, the page will be memset later
    kfree_internal(p + i * PGSIZE, is_locked, no_memset);
  }
}
#endif
