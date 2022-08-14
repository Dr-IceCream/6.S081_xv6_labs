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
  char lockname[10];
} kmems[NCPU];

void
kinit()
{
  for(int i=0;i<NCPU;i++){
    snprintf(kmems[i].lockname, 10, "kmem_%d", i);
    initlock(&kmems[i].lock,kmems[i].lockname);
  }
  // initlock(&kmem.lock, "kmem");
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

// Free the page of physical memory pointed at by v,
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
  int cid=cpuid();
  pop_off();
  
  acquire(&kmems[cid].lock);
  r->next = kmems[cid].freelist;
  kmems[cid].freelist = r;
  release(&kmems[cid].lock);
}

//try to steal other CPUS' freelist 
struct run *steal(int cpu_id) {
  int c = cpu_id ; //from this(next) on to search CPU having freelist
  struct run *fast, *slow, *head;
  if(cpu_id != cpuid()) { //the cpu_id in is not correct
    panic("steal");
  } 
  for (int i = 1; i < NCPU; i++) {
    c++;
    c = c % NCPU;
    acquire(&kmems[c].lock);
    // if the freelist is not null
    if (kmems[c].freelist) {
      // spit the freelist  to half
      slow = head = kmems[c].freelist;
      fast = slow->next;
      while (fast) {
	fast = fast->next;
	if (fast) {
	  slow = slow->next;
	  fast = fast->next;
	}
      }
      kmems[c].freelist = slow->next;
      release(&kmems[c].lock);
      slow->next = 0;
      // return the before half
      return head;
    } else {
      release(&kmems[c].lock);
    }
  }
  // no freelist at all
  return 0;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cid=cpuid();
  pop_off();

  acquire(&kmems[cid].lock);
  r = kmems[cid].freelist;
  if(r)
    kmems[cid].freelist = r->next;

  if(!r){ //if the freelist is null
    r=steal(cid); // try to steal other CPU's
    if(r){ //success to steal
      kmems[cid].freelist=r->next;
    }
  }   
  release(&kmems[cid].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
