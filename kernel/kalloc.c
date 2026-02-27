// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

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

struct {
  struct spinlock lock;
  struct run *freelist;
  uint refcnt[(PHYSTOP - KERNBASE) / PGSIZE];
} kmem;

// Return the index in the RC array of the physical page pointed into by pa.
static uint
kparcidx(void *pa) {
  if((uint64)pa % PGSIZE != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kparc: invalid physical address");
  return ((uint64)pa - KERNBASE) / PGSIZE;
}

static uint
kparc_nolock(void *pa)
{
  return kmem.refcnt[kparcidx(pa)];
}

static uint
kparcinc_nolock(void *pa)
{
  return ++kmem.refcnt[kparcidx(pa)];
}

static uint
kparcdec_nolock(void *pa)
{
  uint idx = kparcidx(pa);
  if(kmem.refcnt[idx] == 0)
    panic("kparcdec");
  return --kmem.refcnt[idx];
}

uint
kparc(void *pa)
{
  uint rc;
  acquire(&kmem.lock);
  rc = kparc_nolock(pa);
  release(&kmem.lock);
  return rc;
}

uint
kparcinc(void *pa)
{
  uint rc;
  acquire(&kmem.lock);
  rc = kparcinc_nolock(pa);
  release(&kmem.lock);
  return rc;
}

uint
kparcdec(void *pa)
{
  uint rc;
  acquire(&kmem.lock);
  rc = kparcdec_nolock(pa);
  release(&kmem.lock);
  return rc;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    kparcinc(p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if(kparcdec_nolock(pa) > 0){
    release(&kmem.lock);
    return;
  }
  r = (struct run*)pa;
  memset(pa, 1, PGSIZE);
  if(kparc_nolock(pa) == 0){
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    if (kparc_nolock(r) != 0)  panic("kalloc: refcnt should be 0");
    kmem.freelist = r->next;
    kparcinc_nolock(r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
