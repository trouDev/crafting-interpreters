#include <stdio.h>
#include "vm.h"

VM vm;

void initVM(void) { vm.stackTop = vm.stack; }
void freeVM(void) {}

static void push(Value value) { *vm.stackTop++ = value; }
static Value pop(void) { return *--vm.stackTop; }
static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value)
{
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static InterpretResult run(void)
{
#define READ_BYTE() (*vm.ip++)
#define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_NUMBER(op) do { \
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
    fprintf(stderr, "Operands must be numbers.\n"); \
    return INTERPRET_RUNTIME_ERROR; \
  } \
  double b = AS_NUMBER(pop()); double a = AS_NUMBER(pop()); \
  push(NUMBER_VAL(a op b)); \
} while (false)

  for (;;)
  {
    uint8_t instruction = READ_BYTE();
    switch (instruction)
    {
    case OP_CONSTANT: push(READ_CONSTANT()); break;
    case OP_ADD: BINARY_NUMBER(+); break;
    case OP_SUBTRACT: BINARY_NUMBER(-); break;
    case OP_MULTIPLY: BINARY_NUMBER(*); break;
    case OP_DIVIDE: BINARY_NUMBER(/); break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) { fprintf(stderr, "Operand must be a number.\n"); return INTERPRET_RUNTIME_ERROR; }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
    case OP_NIL: push(NIL_VAL); break;
    case OP_TRUE: push(BOOL_VAL(true)); break;
    case OP_FALSE: push(BOOL_VAL(false)); break;
    case OP_EQUAL:
    {
      Value b = pop(), a = pop();
      bool equal = false;
      if (a.type == b.type)
      {
        if (IS_NIL(a)) equal = true;
        else if (IS_BOOL(a)) equal = AS_BOOL(a) == AS_BOOL(b);
        else if (IS_NUMBER(a)) equal = AS_NUMBER(a) == AS_NUMBER(b);
      }
      push(BOOL_VAL(equal));
      break;
    }
    case OP_GREATER: BINARY_NUMBER(>); break;
    case OP_LESS: BINARY_NUMBER(<); break;
    case OP_JUMP:
    {
      uint16_t offset = READ_SHORT();
      vm.ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE:
    {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0))) vm.ip += offset;
      break;
    }
    case OP_POP: pop(); break;
    case OP_RETURN:
      printValue(pop()); printf("\n"); return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef BINARY_NUMBER
}

InterpretResult interpret(Chunk *chunk)
{
  vm.chunk = chunk;
  vm.ip = chunk->code;
  return run();
}
