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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // hash bucket
  struct buf buckets[NBUCKET];
  struct spinlock bucket_locks[NBUCKET];
} bcache;

int
hash_func(int input)
{
  return input % NBUCKET;
}

// check if buf with given blockno in bucket
// DON'T NEED TO acquire&release lock in this function.
// the caller of this function need to acquire&release lock.
struct buf*
check_if_buf_in_given_bucket(int blockno)
{
  int bucket_id = hash_func(blockno);

  struct buf* bucket = &bcache.buckets[bucket_id];

  struct buf* curr;
  for (curr = bucket->next; curr != bucket; curr = curr->next) {
    if (curr->blockno == blockno) {
      return curr;
    }
  }

  return 0;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket_locks[i], "bcache.buckets");
    bcache.buckets[i].prev = &bcache.buckets[i];
    bcache.buckets[i].next = &bcache.buckets[i];
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // calculate b's hash_value, use it as bucket id
    int bucket_id = hash_func(b->blockno);
    b->next = bcache.buckets[bucket_id].next;
    b->prev = &bcache.buckets[bucket_id];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[bucket_id].next->prev = b;
    bcache.buckets[bucket_id].next = b;
  }
}

// DON'T NEED TO acquire&release lock
struct buf*
get_smallest_ticks_buf(struct buf* bucket)
{
  uint smallest_ticks = -1;
  struct buf* corresponding_buf = 0;
  for(struct buf* b = bucket->next; b != bucket; b = b->next){
    //printf("ticks: %d\n", b->ticks);
    if (b->refcnt == 0 && b->ticks < smallest_ticks) {
      smallest_ticks = b->ticks;
      corresponding_buf = b;
    }
  }
  //if (smallest_ticks == -1) {
  //  printf("fuck!\n");
  //}
  return corresponding_buf;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket_id = hash_func(blockno);
  acquire(&bcache.bucket_locks[bucket_id]);

  // Is the block already cached?
  if ((b = check_if_buf_in_given_bucket(blockno)) != 0) {
    //printf("wtf?\n");
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[bucket_id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  // bucket_id corresponding bucket has buf
  // => return buf.
  struct buf* bucket = &bcache.buckets[bucket_id];
  struct buf* corresponding_buf = get_smallest_ticks_buf(bucket);
  if(corresponding_buf != 0) {
    corresponding_buf->dev = dev;
    corresponding_buf->blockno = blockno;
    corresponding_buf->valid = 0;
    corresponding_buf->refcnt = 1;
    release(&bcache.bucket_locks[bucket_id]);
    acquiresleep(&corresponding_buf->lock);
    return corresponding_buf;
  }
  release(&bcache.bucket_locks[bucket_id]);

  // Stole other buckets
  for (int i = 0; i < NBUCKET; i++) {
    if (i == bucket_id) { continue; }
    acquire(&bcache.bucket_locks[i]);

    // try to get a buf whose refcnt == 0
    struct buf* bucket_i = &bcache.buckets[i];
    struct buf* corresponding_buf = get_smallest_ticks_buf(bucket_i);
    if (corresponding_buf != 0) {
      corresponding_buf->dev = dev;
      corresponding_buf->blockno = blockno;
      corresponding_buf->valid = 0;
      corresponding_buf->refcnt = 1;

      struct buf* tmp_next = corresponding_buf->next;
      struct buf* tmp_prev = corresponding_buf->prev;
      tmp_next->prev = tmp_prev;
      tmp_prev->next = tmp_next;

      corresponding_buf->next = bucket->next;
      corresponding_buf->prev = bucket;
      bucket->next->prev = corresponding_buf;
      bucket->next = corresponding_buf;

      release(&bcache.bucket_locks[i]);
      acquiresleep(&corresponding_buf->lock);
      return corresponding_buf;
    }

    release(&bcache.bucket_locks[i]);
  }

  panic("bget: no buffers");
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

  releasesleep(&b->lock);

  int bucket_id = hash_func(b->blockno);

  acquire(&bcache.bucket_locks[bucket_id]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->ticks = ticks;
  }
  
  release(&bcache.bucket_locks[bucket_id]);
}

void
bpin(struct buf *b) {
  int bucket_id = hash_func(b->blockno);

  acquire(&bcache.bucket_locks[bucket_id]);
  b->refcnt++;
  release(&bcache.bucket_locks[bucket_id]);
}

void
bunpin(struct buf *b) {
  int bucket_id = hash_func(b->blockno);

  acquire(&bcache.bucket_locks[bucket_id]);
  b->refcnt--;
  release(&bcache.bucket_locks[bucket_id]);
}


