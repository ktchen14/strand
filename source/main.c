#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "strand.h"

void *comain(void *argument, void *ctxt) {
  void *object;

  if ((object = malloc(sizeof(int) * 4)) == NULL)
    return NULL;

  printf("%p\n", argument);

  argument = strand_return(object + 0);
  printf("%p\n", argument);
  argument = strand_return(object + 1);
  printf("%p\n", argument);
  argument = strand_return(object + 2);
  printf("%p\n", argument);
  argument = strand_return(object + 3);
  printf("%p\n", argument);

  free(object);

  return NULL;
}

int main(int argc, char *argv[]) {
  strand_t *strand;

  if ((strand = strand_create(comain, NULL)) == NULL)
    return EXIT_FAILURE;

  for (size_t i = 0; ; i++) {
    void *result;
    if ((result = strand_resume(strand, (char *) strand + i)) == NULL)
      break;
    printf("%p\n", result);
  }

  strand_delete(strand);
}
