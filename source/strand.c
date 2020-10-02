#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "strand.h"

#include "strand-x86-64.h"

struct strand_t {
  strand_t *caller;         // offset = 0x00

  // The size of the strand's stack. This is zero if and only if the strand is a
  // root strand.
  const size_t size;        // offset = 0x08

  void *rsp;                // offset = 0x10
  unsigned char status;     // offset = 0x18
  _Alignas(16) char data[]; // offset = 0x20
};

/// Initialize the @a strand to call @a main with @a ctxt on activation
void strand_initialize(strand_t *strand, strand_main_t main, void *ctxt);

/// Stop and save the @a self strand and continue onto the @a next strand with
/// @a result
void *strand_switch(strand_t **selfp, strand_t *next, void *result);

/// This thread's root strand. This must always have a @c NULL caller and zero
/// size.
static _Thread_local strand_t root = {
  .caller = NULL, .size = 0, .status = STRAND_ACTIVE,
};

/// The active strand or @c NULL if strand_self() hasn't been called yet in this
/// thread.
static _Thread_local strand_t *self = NULL;

#include <limits.h>
#include <unistd.h>

/// Return @a size rounded up to a multiple of the page size or a number less
/// than @a size if this will overflow a size_t
static size_t paginate(size_t size) __attribute__((const));

#if defined(_POSIX_PAGESIZE)
static const size_t pagesize = _POSIX_PAGESIZE;
#elif defined(_PAGESIZE)
static const size_t pagesize = _PAGESIZE;
#else
static size_t pagesize;

#include <stdio.h>

__attribute__((constructor)) static void retrieve_pagesize(void) {
  long sc_pagesize;

  errno = 0;
  if ((sc_pagesize = sysconf(_SC_PAGESIZE)) == -1) {
    if (errno != 0)
      fprintf(stderr, "retrieve_pagesize(): indeterminate page size\n");
    else
      fprintf(stderr, "retrieve_pagesize(): %s\n", strerror(errno));
    abort();
  } else if (sc_pagesize > SIZE_MAX) {
    fprintf(stderr, "retrieve_pagesize(): unreasonable page size\n");
    abort();
  }
  pagesize = sc_pagesize;
}
#endif

strand_t *strand_create(strand_main_t main, void *ctxt) {
  strand_t *strand;

  if ((strand = malloc(sizeof(strand_t) + 8192)) == NULL)
    return NULL;
  memcpy(strand, &(strand_t) {
    .size = 8192, .status = STRAND_BORN,
  }, sizeof(strand_t));

  strand_initialize(strand, main, ctxt);

  return strand;
}

#include <sys/mman.h>

#if defined(MAP_ANONYMOUS) || defined(MAP_ANON)
strand_t *strand_create_safe(strand_main_t main, void *ctxt, size_t size) {
  if (size == 0)
    return errno = EINVAL, NULL;

  size_t strand_size = 0;

  // Layout (each component must be aligned to the page size):
  strand_size += paginate(sizeof(strand_t)); // strand_t
  strand_size += pagesize;                   // guard page
  strand_size += paginate(size);             // stack
  strand_size += pagesize;                   // guard page

  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  char *allocation = mmap(NULL, strand_size, PROT_READ | PROT_WRITE, flags, -1, 0);
  if (allocation == NULL)
    return NULL;

  void *lower_guard = &allocation[paginate(sizeof(strand_t))];
  void *upper_guard = &allocation[strand_size - pagesize];

  if (!mprotect(lower_guard, pagesize, PROT_NONE))
    goto except_mprotect;
  if (!mprotect(upper_guard, pagesize, PROT_NONE))
    goto except_mprotect;

  memcpy(allocation, &(strand_t) {
    .size = paginate(size), .status = STRAND_BORN,
  }, sizeof(strand_t));

  strand_t *strand = (strand_t *) allocation;
  strand_initialize(strand, main, ctxt);
  return strand;

except_mprotect:
  munmap(allocation, strand_size);
  return NULL;
}
#endif

void strand_delete(strand_t *strand) {
  free(strand);
}

void strand_delete_safe(strand_t *strand) {
  munmap(strand, paginate(sizeof(strand_t)) + 2 * pagesize + paginate(strand->size));
}

_Bool strand_is_root(strand_t *strand) {
  return strand->size == 0;
}

strand_t *strand_self() {
  return self = self != NULL ? self : &root;
}

strand_t *strand_root() {
  return &root;
}

void *strand_resume(strand_t *next, void *result) {
  // can't resume a strand that isn't halted (or born)
  assert(next->status == STRAND_HALTED || next->status == STRAND_BORN);

  // can't resume a called strand
  assert(next->caller == NULL);

  // can't resume a root strand
  assert(!strand_is_root(next));

  // arrest and switch
  next->caller = strand_self();
  self->status = STRAND_LIVE;
  return strand_switch(&self, next, result);
}

void *strand_return(void *result) {
  // return to either the caller or the root
  strand_t *next;
  if ((next = strand_self()->caller) == NULL)
    next = &root;

  // halt and switch
  self->status = STRAND_HALTED;
  self->caller = NULL;
  return strand_switch(&self, next, result);
}

void *strand_transfer(strand_t *next, void *result) {
  // can't transfer to a strand that isn't halted (or born)
  assert(next->status == STRAND_HALTED || next->status == STRAND_BORN);

  // can't transfer to a called strand
  assert(next->caller == NULL);

  // can't transfer to a root strand with a caller
  assert(!strand_is_root(next) || strand_self()->caller == NULL);

  next->caller = strand_self()->caller;

  // halt and switch
  self->status = STRAND_HALTED;
  self->caller = NULL;
  return strand_switch(&self, next, result);
}

size_t paginate(size_t size) {
  return pagesize * (size / pagesize + (size % pagesize > 0));
}
