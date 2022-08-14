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
#define HASH(blockno) (blockno % NBUCKET)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  int size;  //record the inserted buf num
  struct buf buckets[NBUCKET];
  struct spinlock locks[NBUCKET];
  struct spinlock hashlock;
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  bcache.size=0;
  initlock(&bcache.hashlock,"bcache_hash");
  for(int i=0;i<NBUCKET;i++){
    initlock(&bcache.locks[i],"bcache_buckets");
  }


  /*// Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;*/
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    /*b->next = bcache.head.next;
    b->prev = &bcache.head;*/
    initsleeplock(&b->lock, "buffer");
    /*bcache.head.next->prev = b;
    bcache.head.next = b;*/
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int index = HASH(blockno);
  struct buf *pre, *LRUb=0, *LRUpre=0;
  uint64 mintimestamp;

  acquire(&bcache.locks[index]);
  // Is the block already cached?
  for(b = bcache.buckets[index].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  acquire(&bcache.lock);
  if(bcache.size < NBUF){ // there are unused space
    b = &bcache.buf[bcache.size++];
    release(&bcache.lock);
    b->dev=dev;
    b->blockno=blockno;
    b->valid=0;
    b->refcnt=1;
    b->next=bcache.buckets[index].next;
    bcache.buckets[index].next=b;
    release(&bcache.locks[index]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  release(&bcache.locks[index]);
  acquire(&bcache.hashlock);
  for(int i=0;i<NBUCKET;i++){
    mintimestamp=-1; // set to maxvalue (because it is unsigned int)
    acquire(&bcache.locks[index]);
    pre=&bcache.buckets[index];
    b=pre->next;
    while(b){ // find the LRUb
      if(b->refcnt==0 && b->timestamp<mintimestamp){
	LRUb=b;
	LRUpre=pre;
	mintimestamp=b->timestamp;
      }
      pre=b;
      b=b->next;
    }
    if(LRUb){ // have found a valid buf
      LRUb->dev=dev;
      LRUb->blockno=blockno;
      LRUb->valid=0;
      LRUb->refcnt=1;
      //if it is in other bucket, move it
      if(index!=HASH(blockno)){
	LRUpre->next=LRUb->next;
	release(&bcache.locks[index]);
	index=HASH(blockno);
	acquire(&bcache.locks[index]);
	LRUb->next=bcache.buckets[index].next;
	bcache.buckets[index].next=LRUb;
      }
      release(&bcache.locks[index]);
      release(&bcache.hashlock);
      acquiresleep(&LRUb->lock);
      return LRUb;
    }
    release(&bcache.locks[index]);
    index++;
    index = index%NBUCKET;
  }

        
  /*// Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }*/
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

  int index = HASH(b->blockno);
  acquire(&bcache.locks[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
    /*// no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;*/
  }
  
  release(&bcache.locks[index]);
}

void
bpin(struct buf *b) {
  int index=HASH(b->blockno);
  acquire(&bcache.locks[index]);
  b->refcnt++;
  release(&bcache.locks[index]);
}

void
bunpin(struct buf *b) {
  int index=HASH(b->blockno);
  acquire(&bcache.locks[index]);
  b->refcnt--;
  release(&bcache.locks[index]);
}


