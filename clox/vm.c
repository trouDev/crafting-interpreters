#include <stdio.h>

#include "memory.h"
#include "vm.h"

VM vm;

void initVM(void) {
  vm.stackCapacity = 0;
  vm.stack = NULL;
  vm.stackTop = NULL;
}

void freeVM(void) {
  FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
  initVM();
}

static void growStack(void) {
  int oldCapacity = vm.stackCapacity;
  vm.stackCapacity = GROW_CAPACITY(oldCapacity);
  int stackCount = vm.stack == NULL ? 0 : (int)(vm.stackTop - vm.stack);
  vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
  vm.stackTop = vm.stack + stackCount;
}

static void push(Value value) {
  if (vm.stack == NULL ||
      vm.stackTop - vm.stack >= vm.stackCapacity) {
    growStack();
  }
  *vm.stackTop = value;
  vm.stackTop++;
}

static Value pop(void) {
  vm.stackTop--;
  return *vm.stackTop;
}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
  do { \
    Value b = pop(); \
    Value a = pop(); \
    push(a op b); \
  } while (false)

  for (;;) {
    uint8_t instruction = READ_BYTE();
    switch (instruction) {
      case OP_CONSTANT:
        push(READ_CONSTANT());
        break;
      case OP_ADD:      BINARY_OP(+); break;
      case OP_SUBTRACT: BINARY_OP(-); break;
      case OP_MULTIPLY: BINARY_OP(*); break;
      case OP_DIVIDE:   BINARY_OP(/); break;
      case OP_NEGATE:
        push(-pop());
        break;
      case OP_RETURN:
        printValue(pop());
        printf("\n");
        return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}
