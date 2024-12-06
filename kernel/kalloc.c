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
  struct spinlock lock[NCPU];
  struct run *freelist[NCPU];
  int freelist_count[NCPU];
} kmem;

void
kinit()
{
  for (int i = 0; i < NCPU; i++) {
    char lockname[10];
    snprintf(lockname, 10, "kmem%d", i);
    initlock(&kmem.lock[i], lockname);
    kmem.freelist_count[i] = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  int memlist_id = 0;
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    struct run *r;

    // Fill with junk to catch dangling refs.
    memset(p, 1, PGSIZE);

    r = (struct run*)p;

    acquire(&kmem.lock[memlist_id]);
    r->next = kmem.freelist[memlist_id];
    kmem.freelist[memlist_id] = r;
    kmem.freelist_count[memlist_id]++;
    release(&kmem.lock[memlist_id]);

    memlist_id = (memlist_id + 1) % NCPU;
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int cpu_id = cpuid();

  acquire(&kmem.lock[cpu_id]);
  r->next = kmem.freelist[cpu_id];
  kmem.freelist[cpu_id] = r;
  kmem.freelist_count[cpu_id]++;
  release(&kmem.lock[cpu_id]);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();
  int cpu_id = cpuid();

  for(int i = 0; i<NCPU; i++){
    int memlist_id = (cpu_id + i) % NCPU;
    if(kmem.freelist_count[memlist_id] == 0){
      continue;
    }

    acquire(&kmem.lock[memlist_id]);
    if(kmem.freelist_count[memlist_id] == 0){
      release(&kmem.lock[memlist_id]);
      continue;
    }
    r = kmem.freelist[memlist_id];
    kmem.freelist[memlist_id] = r->next;
    kmem.freelist_count[memlist_id]--;
    release(&kmem.lock[memlist_id]);
    break;
  }
  pop_off();


  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
