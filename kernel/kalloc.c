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

void dokfree(void *pa);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct spinlock pgcntlock;
char pgcnt[NPHYPAGE];
int pgcntidx(void *pa){
  return ((uint64)pa - KERNBASE) / PGSIZE;
}

int kpagecnt(void *pa){
  return pgcnt[pgcntidx(pa)];
}

void kpageinc(void *pa){
  acquire(&pgcntlock);
  pgcnt[pgcntidx(pa)]++;
  release(&pgcntlock);
}

void kpagedec(void *pa){
  acquire(&pgcntlock);
  pgcnt[pgcntidx(pa)]--;
  release(&pgcntlock);
}

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgcntlock, "pgcnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    dokfree(p);
    pgcnt[pgcntidx(p)] = 0;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void dokfree(void *pa){
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void
kfree(void *pa)
{
  if(kpagecnt(pa) > 1){
    kpagedec(pa);
    return;
  } else if(kpagecnt(pa) == 1){
    kpagedec(pa);
  } else {
    panic("kfree: page count is not zero");
  }

  dokfree(pa);
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
    kmem.freelist = r->next;
    if(pgcnt[pgcntidx(r)] != 0){
      panic("kalloc: page count is not zero");
    }
    kpageinc(r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
