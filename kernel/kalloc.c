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

// refNum[i] 表示物理页面 i 的引用计数
struct {
  struct spinlock lock;
  int refNum[PHYSTOP / PGSIZE];
} refNum_t;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 得到refNum_t的锁
void
acquireRefNumLock()
{
  acquire(&refNum_t.lock);
}

// 释放refNum_t的锁
void
releaseRefNumLock()
{
  release(&refNum_t.lock);
}

// 获取物理页面pa的引用计数
int
getRefNum(void *pa)
{
  acquire(&refNum_t.lock);
  int cnt = refNum_t.refNum[(uint64)pa / PGSIZE];
  release(&refNum_t.lock);
  return cnt;
}

// 增加物理页面pa的引用计数
void
addRefNum(void *pa)
{
  acquire(&refNum_t.lock);
  refNum_t.refNum[(uint64)pa / PGSIZE]++;
  release(&refNum_t.lock);
}

// 减少物理页面pa的引用计数
void
subRefNum(void *pa)
{
  acquire(&refNum_t.lock);
  refNum_t.refNum[(uint64)pa / PGSIZE]--;
  release(&refNum_t.lock);
}

// 设置物理页面pa的引用计数为1
void 
setRefNum(void *pa)
{
  acquire(&refNum_t.lock);
  refNum_t.refNum[(uint64)pa / PGSIZE] = 1;
  release(&refNum_t.lock);
}


void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refNum_t.lock, "refNum_t");
  memset(refNum_t.refNum, 0, sizeof(refNum_t.refNum));
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

  if (getRefNum(pa) > 1) {
    subRefNum(pa);
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
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
  if(r)
    {
      kmem.freelist = r->next;
      setRefNum((void*)r); // 设置引用计数为1
    }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
