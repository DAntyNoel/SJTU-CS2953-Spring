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
  char lockname[8];
} kmem[NCPU];

void
kinit()
{
  for (int i = 0;i < NCPU;i++){
    snprintf(kmem[i].lockname, 8, "kmem_%d", i);
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int i = cpuid();
  pop_off();

  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  release(&kmem[i].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
struct run *
steal(int cpu_id) 
{
  int i;
  int c = cpu_id;
  struct run *fast, *slow, *head;
  for (i = 1; i < NCPU; ++i) {
    if (++c == NCPU) {
      c = 0;
    }
    acquire(&kmem[c].lock);
    if (kmem[c].freelist) {
      slow = head = kmem[c].freelist;
      fast = slow->next;
      while (fast) {
        fast = fast->next;
        if (fast) {
          slow = slow->next;
          fast = fast->next;
        }
      }
      kmem[c].freelist = slow->next;
      release(&kmem[c].lock);
      slow->next = 0;
      return head;
    }
    release(&kmem[c].lock);
  }
  return 0;
}

void *
kalloc(void)
{
  struct run *r;
  push_off();
  int i = cpuid();
  pop_off();

  acquire(&kmem[i].lock);
  r = kmem[i].freelist;
  if(r)
    kmem[i].freelist = r->next;
  else{
    r = steal(i);
    if(r)
      kmem[i].freelist = r->next;
  }
  release(&kmem[i].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
