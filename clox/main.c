#include "chunk.h"
#include "vm.h"

static void writeConstant(Chunk* chunk, double value, int line) {
  int constant = addConstant(chunk, value);
  writeChunk(chunk, OP_CONSTANT, line);
  writeChunk(chunk, (uint8_t)constant, line);
}

int main(int argc, const char* argv[]) {
  (void)argc;
  (void)argv;

  initVM();
  Chunk chunk;
  initChunk(&chunk);

  writeConstant(&chunk, 1.0, 123);
  for (int i = 1; i < 300; i++) {
    writeConstant(&chunk, 1.0, 123);
    writeChunk(&chunk, OP_ADD, 123);
  }
  writeChunk(&chunk, OP_RETURN, 123);

  interpret(&chunk);
  freeChunk(&chunk);
  freeVM();
  return 0;
}
