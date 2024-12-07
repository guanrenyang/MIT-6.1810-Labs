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

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct spinlock bucket_locks[NBUCKET];
  struct buf head[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for(int i = 0; i < NBUCKET; i++){
    bcache.head[i].next = &bcache.head[i];
    bcache.head[i].prev = &bcache.head[i];

    char lock_name[16];
    snprintf(lock_name, sizeof(lock_name), "bcache_%d", i);
    initlock(&bcache.bucket_locks[i], lock_name);
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int bucket_id = (b - bcache.buf) % NBUCKET;
    b->next = bcache.head[bucket_id].next;
    b->prev = &bcache.head[bucket_id];
    bcache.head[bucket_id].next->prev = b;
    bcache.head[bucket_id].next = b;
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket_id = blockno % NBUCKET;

  acquire(&bcache.bucket_locks[bucket_id]);
  // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  for(b = bcache.head[bucket_id].next; b != &bcache.head[bucket_id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[bucket_id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
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
  for(b = bcache.head[bucket_id].next; b != &bcache.head[bucket_id]; b = b->next){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.bucket_locks[bucket_id]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[bucket_id]);

  struct buf *empty_b;
  for(int i=0;;i++){
    int tmp_bucket_id = i % NBUCKET;
    acquire(&bcache.bucket_locks[tmp_bucket_id]);
    for (empty_b=bcache.head[tmp_bucket_id].next;empty_b!=&bcache.head[tmp_bucket_id];empty_b=empty_b->next){
      if(empty_b->refcnt==0){
        empty_b->prev->next = empty_b->next;
        empty_b->next->prev = empty_b->prev;
        release(&bcache.bucket_locks[tmp_bucket_id]);
        goto found_empty;
      }
    }
    release(&bcache.bucket_locks[tmp_bucket_id]);
  }
  panic("bget: no buffers");
found_empty:
  acquire(&bcache.bucket_locks[bucket_id]);
  empty_b->prev = &bcache.head[bucket_id];
  empty_b->next = bcache.head[bucket_id].next;
  bcache.head[bucket_id].next->prev = empty_b;
  bcache.head[bucket_id].next = empty_b;
  empty_b->dev = dev;
  empty_b->blockno = blockno;
  empty_b->valid = 0;
  empty_b->refcnt = 1;
  release(&bcache.bucket_locks[bucket_id]);
  acquiresleep(&empty_b->lock);
  return empty_b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
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

  b->refcnt--;
  
  releasesleep(&b->lock);
}

void
bpin(struct buf *b) {
  int bucket_id = b->blockno % NBUCKET;
  acquire(&bcache.bucket_locks[bucket_id]);
  b->refcnt++;
  release(&bcache.bucket_locks[bucket_id]);
}

void
bunpin(struct buf *b) {
  int bucket_id = b->blockno % NBUCKET;
  acquire(&bcache.bucket_locks[bucket_id]);
  b->refcnt--;
  release(&bcache.bucket_locks[bucket_id]);
}


