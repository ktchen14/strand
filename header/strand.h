#include <stddef.h>

typedef struct strand_t strand_t;

typedef enum strand_status_t {
  /// The strand is running. At any time each thread has at most one active
  /// strand.
  STRAND_ACTIVE = 0,

  /// Like STRAND_HALTED but the strand's never been active
  STRAND_BORN,

  /// The strand is inactive and isn't the caller of another strand. It's able
  /// to be resumed or transferred to.
  STRAND_HALTED,

  /// The strand is inactive and is the caller of another strand. It can't be
  /// resumed or transferred to until its callee does a strand_return().
  STRAND_LIVE,

  /// The strand has exited. It can't be resumed or transferred to.
  STRAND_DEAD,
} strand_status_t;

typedef void *(*strand_main_t)(void *argument, void *ctxt);

/**
 * @brief Allocate a strand on the heap and initialize it to call @a main with
 *   @a ctxt
 *
 * The created strand will have guard pages around its stack.
 */
strand_t *strand_create(strand_main_t main, void *ctxt)
  __attribute__((malloc, nonnull(1)));

/**
 * @brief Allocate a strand on the heap and initialize it to call @a main with
 *   @a ctxt
 */
strand_t *strand_create_simple(strand_main_t main, void *ctxt)
  __attribute__((malloc, nonnull(1)));

/**
 * @brief Delete the @a strand
 *
 * Once a strand is deleted, it can't be resumed, returned to, or transferred
 * to. Thus the strand should be halted or dead when its deleted.
 */
void strand_delete(strand_t *strand);

/// Return the active strand in this thread
strand_t *strand_self() __attribute__((returns_nonnull));

/// Return the root strand of this thread
strand_t *strand_root() __attribute__((returns_nonnull));

/**
 * @brief Return whether the @a strand is a root strand
 *
 * This is different from the expression <code>strand == strand_root()</code> as
 * that will evaluate to whether @c strand is the root strand of this thread,
 * while this will return whether @a strand is the root strand of @b any thread.
 *
 * @param strand a strand
 * @return whether @a strand is a root strand
 */
_Bool strand_is_root(strand_t *strand) __attribute__((nonnull, pure));

/// Return the status of the @a strand
strand_status_t strand_status(strand_t *strand) __attribute__((nonnull, pure));

/// Return the caller of the @a strand
strand_t *strand_caller(strand_t *strand) __attribute__((nonnull, pure));

/// Can't be called on a live or dead strand
void *strand_resume(strand_t *strand, void *result);

/// Transfer to strand. The @a strand's caller becomes this strand's caller.
/// This strand becomes halted.
void *strand_transfer(strand_t *strand, void *result);

/**
 * @brief Return @a result from this strand
 *
 * If the strand has a caller, then this will return to that strand. Otherwise,
 * this will return to the root strand of this thread.
 *
 * The active strand will be halted and it's caller will be nulled.
 */
void *strand_return(void *result);
