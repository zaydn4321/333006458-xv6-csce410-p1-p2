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

#define NFRAMES ((PHYSTOP - KERNBASE) / PGSIZE)
#define FRAME(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  struct run *freelist;
  uint8 freemap[NFRAMES / 8]; // 1 bit per frame, set = on freelist
} kmem;

static int
isfree(int f)
{
  return kmem.freemap[f / 8] & (1 << (f % 8));
}

static void
setfree(int f, int free)
{
  if (free)
    kmem.freemap[f / 8] |= 1 << (f % 8);
  else
    kmem.freemap[f / 8] &= ~(1 << (f % 8));
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
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

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  if (isfree(FRAME(pa)))
    panic("kfree: double free");
  setfree(FRAME(pa), 1);
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
  if (r) {
    kmem.freelist = r->next;
    setfree(FRAME(r), 0);
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// Allocate n physically contiguous pages.
// Returns the address of the first page, or 0 if no run of n free pages exists.
void *
kalloc_contiguous(int n)
{
  int f, len = 0;
  char *pa;
  struct run **pp;

  if (n <= 0)
    return 0;

  acquire(&kmem.lock);
  for (f = 0; f < NFRAMES && len < n; f++)
    len = isfree(f) ? len + 1 : 0;
  if (len < n) {
    release(&kmem.lock);
    return 0;
  }

  // f is one past the end of the run
  pa = (char *)(KERNBASE + (uint64)(f - n) * PGSIZE);
  for (pp = &kmem.freelist; *pp;) {
    if ((char *)*pp >= pa && (char *)*pp < pa + (uint64)n * PGSIZE)
      *pp = (*pp)->next;
    else
      pp = &(*pp)->next;
  }
  for (f -= n; len > 0; len--, f++)
    setfree(f, 0);
  release(&kmem.lock);

  memset(pa, 5, (uint64)n * PGSIZE);
  return pa;
}

void
kfree_contiguous(void *pa, int n)
{
  for (int i = 0; i < n; i++)
    kfree((char *)pa + (uint64)i * PGSIZE);
}

#ifdef KALLOC_TEST
#define CHECK(c) if (!(c)) panic("kalloctest: " #c)

static int
nfree(void)
{
  int f, c = 0;

  acquire(&kmem.lock);
  for (f = 0; f < NFRAMES; f++)
    if (isfree(f))
      c++;
  release(&kmem.lock);
  return c;
}

// is any page of [pa, pa + n pages) still on the freelist.
// junk pointers count too, since memset over a listed page wrecks the list
static int
onfreelist(char *pa, int n)
{
  struct run *r;
  int found = 0;

  acquire(&kmem.lock);
  for (r = kmem.freelist; r && !found; r = r->next)
    if ((char *)r < end || (uint64)r >= PHYSTOP ||
        ((char *)r >= pa && (char *)r < pa + (uint64)n * PGSIZE))
      found = 1;
  release(&kmem.lock);
  return found;
}

// runs at boot right after kinit, so all free memory is one contiguous run
void
kalloctest(void)
{
  int total = nfree(), i;
  char *a, *p;
  struct run *all = 0, *odd = 0, *r, *next;

  // bad sizes
  CHECK(kalloc_contiguous(0) == 0);
  CHECK(kalloc_contiguous(-1) == 0);
  CHECK(kalloc_contiguous(total + 1) == 0);
  CHECK(nfree() == total);

  // basic 3 page block
  a = kalloc_contiguous(3);
  CHECK(a != 0 && (uint64)a % PGSIZE == 0);
  CHECK(nfree() == total - 3);
  for (i = 0; i < 3; i++)
    CHECK(!isfree(FRAME(a + i * PGSIZE)));
  CHECK(!onfreelist(a, 3));
  memset(a, 7, 3 * PGSIZE);
  p = kalloc();
  CHECK(p != 0 && (p < a || p >= a + 3 * PGSIZE));
  kfree(p);
  for (i = 0; i < 3 * PGSIZE; i++)
    CHECK(a[i] == 7);
  kfree_contiguous(a, 3);
  CHECK(nfree() == total);

  // exact fit: whole free range, then nothing left
  a = kalloc_contiguous(total);
  CHECK(a != 0 && nfree() == 0);
  CHECK(kalloc() == 0 && kalloc_contiguous(1) == 0);
  kfree_contiguous(a, total);
  CHECK(nfree() == total);

  // fragmentation: free only even frames, so no 2 free frames touch
  while ((r = kalloc()) != 0) {
    r->next = all;
    all = r;
  }
  for (r = all; r; r = next) {
    next = r->next;
    if (FRAME(r) % 2 == 0) {
      kfree(r);
    } else {
      r->next = odd;
      odd = r;
    }
  }
  CHECK(nfree() > 0);
  CHECK(kalloc_contiguous(2) == 0);
  a = kalloc_contiguous(1);
  CHECK(a != 0);
  kfree(a);
  for (r = odd; r; r = next) {
    next = r->next;
    kfree(r);
  }
  CHECK(nfree() == total);
  a = kalloc_contiguous(total); // merged back into one run
  CHECK(a != 0);
  kfree_contiguous(a, total);
  CHECK(nfree() == total);
  printk("kalloctest: OK\n");
}
#endif
