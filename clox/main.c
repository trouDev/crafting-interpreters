#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
  (void)argc;
  (void)argv;

  Chunk chunk;
  initChunk(&chunk);
  writeConstant(&chunk, 1.2, 123);
  for (int i = 0; i < 255; i++) {
    addConstant(&chunk, (double)i);
  }
  writeConstant(&chunk, 99.0, 123);
  writeChunk(&chunk, OP_RETURN, 123);

  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);
  return 0;
}
