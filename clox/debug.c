#include <stdio.h>

#include "debug.h"

static int simpleInstruction(const char* name, int offset) {
  printf("%-16s\n", name);
  return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int longConstantInstruction(const char* name,
                                   Chunk* chunk, int offset) {
  uint32_t constant = chunk->code[offset + 1] |
                      ((uint32_t)chunk->code[offset + 2] << 8) |
                      ((uint32_t)chunk->code[offset + 3] << 16);
  printf("%-16s %4u '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 4;
}

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && getLine(chunk, offset) == getLine(chunk, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", getLine(chunk, offset));
  }

  switch (chunk->code[offset]) {
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_CONSTANT_LONG:
      return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);
    default:
      printf("Unknown opcode %d\n", chunk->code[offset]);
      return offset + 1;
  }
}
