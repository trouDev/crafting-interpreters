#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

#define STACK_INITIAL_CAPACITY 256

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  Chunk* chunk;
  uint8_t* ip;
  int stackCapacity;
  Value* stack;
  Value* stackTop;
} VM;

extern VM vm;

void initVM(void);
void freeVM(void);
InterpretResult interpret(Chunk* chunk);

#endif
