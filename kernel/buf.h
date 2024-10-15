struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  // struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];

  // 不再使用 LRU cache list来维护缓存的顺序，而是使用时间戳来维护缓存的顺序
  uint timestamp;
};

