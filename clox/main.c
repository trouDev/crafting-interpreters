#include <stdio.h>
#include "compiler.h"
#include "vm.h"

int main(void)
{
  char source[65536];
  size_t length = fread(source, 1, sizeof(source) - 1, stdin);
  source[length] = '\0';

  initVM();
  Chunk chunk;
  initChunk(&chunk);
  if (compile(source, &chunk)) interpret(&chunk);
  freeChunk(&chunk);
  freeVM();
  return 0;
}
