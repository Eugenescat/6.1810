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
} kmem;

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
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

#define NUM_SUPERPAGES 10  // 定义超级页的数量(handful)
static char *superpages[NUM_SUPERPAGES];
static int superpage_used[NUM_SUPERPAGES];  // 标记超级页是否已被使用

void superinit() {
  for (int i = 0; i < NUM_SUPERPAGES; i++) {
      superpages[i] = (char *)SUPERPGROUNDUP((uint64)kalloc());  // 确保每个超级页是 2MB 对齐
      superpage_used[i] = 0;  // 初始化为未使用
  }
}

void *superalloc(void) 
{
  for (int i = 0; i < NUM_SUPERPAGES; i++) {
      if (!superpage_used[i]) {  // 找到一个未使用的超级页
          superpage_used[i] = 1;  // 标记为已使用
          return (void *)superpages[i];
      }
  }
  return 0;  // 如果没有可用的超级页，则返回 NULL
}

void superfree(void *ptr) 
{
  for (int i = 0; i < NUM_SUPERPAGES; i++) {
      if (superpages[i] == ptr) {  // 找到对应的超级页
          superpage_used[i] = 0;  // 标记为未使用
          return;
      }
  }
  memset(ptr, 1, SUPERPGSIZE);
}
