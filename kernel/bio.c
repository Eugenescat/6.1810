// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUCKET 10 // 表示散列表（hash table）的桶（bucket）数量
#define NBUFF (NBUCKET * 3) // 表示缓存中总的块数，它是 NBUCKET 的三倍，用于定义缓存中存储的块数量，并与 NBUCKET 相关。这是为了确保每个桶有大致均衡的块数量（不能大于系统中的NBUF）

struct bucket {
  struct spinlock lock;
  struct buf head;
} hashtable[NBUCKET];

int
hash(uint dev, uint blockno)
{
  return blockno % NBUCKET;
}

struct {
  struct spinlock lock;
  struct buf buf[NBUFF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  // 初始化全局锁 bcache.lock，用于保护整个缓存系统
  initlock(&bcache.lock, "bcache");

  // 初始化每个缓存块的睡眠锁，用于控制对单个缓存块的独立访问
  // 这样可以使得在高并发访问时减少全局锁的争用，尽量做到细粒度控制
  for(b = bcache.buf; b < bcache.buf+NBUFF; b++){
    initsleeplock(&b->lock, "buffer");
  }

  // 初始化分桶（hash table）机制，减少锁争用
  b = bcache.buf;
  for (int i = 0; i < NBUCKET; i++) {
    // 初始化每个桶的锁，这样不同桶之间的操作可以并行进行
    initlock(&hashtable[i].lock, "bcache_bucket");

    // 将缓存块按桶的顺序分配到哈希表中，使每个桶的缓存块链表有一个初始状态
    for (int j = 0; j < NBUFF / NBUCKET; j++) {
      b->blockno = i; // 设置缓存块的块号，使哈希值与桶的索引相对应
      // 将当前缓存块插入到当前桶的链表头部
      b->next = hashtable[i].head.next;
      hashtable[i].head.next = b;
      b++; // 指向下一个缓存块
    }
  }

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // 计算当前块号的哈希索引，找到对应的桶
  int idx = hash(dev, blockno);
  struct bucket* bucket = hashtable + idx;
  acquire(&bucket->lock);

  // 检查当前桶中是否已经缓存了目标块
  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){ // 找到匹配的块
      b->refcnt++; // 引用计数加1
      release(&bucket->lock); // 释放桶锁
      acquiresleep(&b->lock); // 获取块的锁，确保独占访问
      return b; // 返回缓存的块
    }
  }
  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // Not cached.
  // 当前桶中没有缓存目标块，需要找到一个可替换的块
  int min_time = 0x8fffffff; // 初始化最小时间戳为一个很大的值
  struct buf* replace_buf = 0; // 用于存储找到的可替换块

  // 在当前桶中查找引用计数为0且时间戳最小的块
  for(b = bucket->head.next; b != 0; b = b->next){
    if(b->refcnt == 0 && b->timestamp < min_time) { // 该块未被引用且时间戳最小
      replace_buf = b;
      min_time = b->timestamp; // 更新最小时间戳
    }
  }
  if(replace_buf) { // 如果找到合适的块
    goto find; // 跳到找到块的处理逻辑
  }

  // 在当前桶找不到合适的块，尝试在其他桶中查找
  acquire(&bcache.lock); // 获取全局锁以保证全局一致性
  refind:
  for(b = bcache.buf; b < bcache.buf + NBUFF; b++) { // 遍历所有缓冲区
    if(b->refcnt == 0 && b->timestamp < min_time) { // 找到一个未被引用且时间戳最小的块
      replace_buf = b;
      min_time = b->timestamp; // 更新最小时间戳
    }
  }

  if (replace_buf) { // 如果找到一个合适的替换块
    // 从旧的桶中移除该块
    int ridx = hash(replace_buf->dev, replace_buf->blockno); // 计算旧桶的哈希索引
    acquire(&hashtable[ridx].lock); // 获取旧桶的锁
    // 检查该块是否被其他进程使用
    if(replace_buf->refcnt != 0) {
      release(&hashtable[ridx].lock);
      goto refind; // 如果该块在其他桶中已被使用，重新查找
    }
    struct buf *pre = &hashtable[ridx].head; // 前一个块的指针
    struct buf *p = hashtable[ridx].head.next; // 当前块的指针
    while (p != replace_buf) { // 查找到该块的位置
      pre = pre->next;
      p = p->next;
    }
    pre->next = p->next; // 从链表中移除该块
    release(&hashtable[ridx].lock); // 释放旧桶的锁
    // 将该块添加到当前桶
    replace_buf->next = hashtable[idx].head.next;
    hashtable[idx].head.next = replace_buf;
    release(&bcache.lock); // 释放全局锁
    goto find; // 跳到找到块的处理逻辑
  }
  else {
    // 如果所有桶都没有可用的缓冲区，抛出错误
    panic("bget: no buffers");
  }


  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  find:
  // 初始化找到的块，将其映射到当前设备和块号
  replace_buf->dev = dev;
  replace_buf->blockno = blockno;
  replace_buf->valid = 0; // 设置为无效，表示数据需要重新加载
  replace_buf->refcnt = 1; // 更新引用计数
  release(&bucket->lock); // 释放当前桶的锁
  acquiresleep(&replace_buf->lock); // 获取找到块的锁

  return replace_buf; // 返回找到的块
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno); // 这个 bget() 会首先判断是否之前已经缓存过了硬盘中的这个块。如果有，那就直接返回对应的缓存，如果没有，会去找到一个最长时间没有使用的缓存，并且把那个缓存分配给当前块
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int idx = hash(b->dev, b->blockno);

  acquire(&hashtable[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->timestamp = ticks; // 不再用LRU维护，用时间戳维护
  }
  
  release(&hashtable[idx].lock);
}

void
bpin(struct buf *b) {
  int idx = hash(b->dev, b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt++;
  release(&hashtable[idx].lock);
}

void
bunpin(struct buf *b) {
  int idx = hash(b->dev, b->blockno);
  acquire(&hashtable[idx].lock);
  b->refcnt--;
  release(&hashtable[idx].lock);
}


