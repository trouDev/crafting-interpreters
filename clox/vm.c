#include <stdio.h>
#include <string.h>
#include "vm.h"
#include "memory.h"

VM vm;

void initVM(void)
{
  vm.stackTop = vm.stack;
  vm.switchTop = 0;
  vm.objects = NULL;
  initTable(&vm.globals);
}

void freeVM(void)
{
  freeTable(&vm.globals);
  freeObjects(vm.objects);
}

static void push(Value value) { *vm.stackTop++ = value; }
static Value pop(void) { return *--vm.stackTop; }
static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isFalsey(Value value)
{
  if (IS_NIL(value)) return true;
  if (IS_BOOL(value)) return !AS_BOOL(value);
  // This compact clox variant represents comparisons as numeric 0/1 values.
  if (IS_NUMBER(value)) return AS_NUMBER(value) == 0;
  return false;
}

static InterpretResult run(void)
{
#define READ_BYTE() (*vm.ip++)
#define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_NUMBER(op) do { \
  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
    fprintf(stderr, "Operands must be numbers.\n"); \
    return INTERPRET_RUNTIME_ERROR; \
  } \
  double b = AS_NUMBER(pop()); double a = AS_NUMBER(pop()); \
  push(NUMBER_VAL(a op b)); \
} while (false)

#define BINARY_STRING() do { \
  ObjString *b = AS_STRING(pop()); \
  ObjString *a = AS_STRING(pop()); \
  int length = a->length + b->length; \
  char *chars = ALLOCATE(char, length + 1); \
  memcpy(chars, a->chars, a->length); \
  memcpy(chars + a->length, b->chars, b->length); \
  chars[length] = '\0'; \
  push(OBJ_VAL(takeString(chars, length))); \
} while (false)

  for (;;)
  {
    uint8_t instruction = READ_BYTE();
    switch (instruction)
    {
    case OP_CONSTANT: push(READ_CONSTANT()); break;
    case OP_ADD:
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) BINARY_STRING();
      else BINARY_NUMBER(+);
      break;
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
    case OP_DEFINE_GLOBAL:
    {
      ObjString *name = READ_STRING();
      tableSet(&vm.globals, name, pop());
      break;
    }
    case OP_GET_GLOBAL:
    {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value))
      {
        fprintf(stderr, "Undefined variable '%s'.\n", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_SET_GLOBAL:
    {
      ObjString *name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0)))
      {
        tableDelete(&vm.globals, name);
        fprintf(stderr, "Undefined variable '%s'.\n", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_GET_LOCAL: push(vm.stack[READ_BYTE()]); break;
    case OP_SET_LOCAL: vm.stack[READ_BYTE()] = peek(0); break;
    case OP_SWITCH_PUSH: vm.switchStack[vm.switchTop++] = pop(); break;
    case OP_SWITCH_GET: push(vm.switchStack[vm.switchTop - 1]); break;
    case OP_SWITCH_POP: vm.switchTop--; break;
    case OP_PRINT: printValue(pop()); printf("\n"); break;
    case OP_LOOP: { uint16_t offset = READ_SHORT(); vm.ip -= offset; break; }
    case OP_POP: pop(); break;
    case OP_RETURN:
      printValue(pop()); printf("\n"); return INTERPRET_OK;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_NUMBER
#undef BINARY_STRING
}

InterpretResult interpret(Chunk *chunk)
{
  vm.chunk = chunk;
  vm.ip = chunk->code;
  return run();
}
