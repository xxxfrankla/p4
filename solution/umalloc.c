#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
    int heap_type; // 0 for base, 1 for huge
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

static Header base_huge;
static Header *freep_huge;

void
free_base(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

// Free memory from huge heap
void
free_huge(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep_huge; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep_huge = p;
}

static Header*
morecore_base(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  return freep;
}
//Morecore for huge pages
static Header*
morecore_huge(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free_huge((void*)(hp + 1));
  return freep_huge;
}

void*
malloc_base(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      p->s.heap_type = 0;  // Mark as base heap
      freep = prevp;
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore_base(nunits)) == 0)
        return 0;
  }
}

void*
malloc_huge(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep_huge) == 0){
    base_huge.s.ptr = freep_huge = prevp = &base_huge;
    base_huge.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      p->s.heap_type = 1;  // Mark as huge heap
      freep_huge = prevp;
      return (void*)(p + 1);
    }
    if(p == freep_huge)
      if((p = morecore_huge(nunits)) == 0)
        return 0;
  }
}

// vmalloc: Allocate memory based on flags
void*
vmalloc(int nbytes, int flag)
{
  if (flag == VMALLOC_SIZE_BASE) {
    return malloc_base(nbytes);
  } else if (flag == VMALLOC_SIZE_HUGE) {
    return malloc_huge(nbytes);
  } else {
    printf(1, "Please pass VMALLOC_SIZE_BASE or VMALLOC_SIZE_HUGE as flag.\n");
    exit();
  }
}

// vfree: Free memory based on heap_type in header
void
vfree(void *ap)
{
  if ((uint)ap >= KERNBASE) {
    printf(1, "Error - trying to free a kernel pointer in userspace\n");
    exit();
  }
  Header *bp = (Header*)ap - 1;
  if (bp->s.heap_type == 0) {
    free_base(ap);
  } else if (bp->s.heap_type == 1) {
    free_huge(ap);
  } else {
    printf(1, "Invalid heap_type in vfree\n");
    exit();
  }
}

void*
malloc(uint nbytes)
{
  int thp_enabled = getthp();
  if (thp_enabled && nbytes >= 0x100000){
    return malloc_huge(nbytes);
  } else {
    return malloc_base(nbytes);
  }
}

void
free(void *ap){
  vfree(ap);
}