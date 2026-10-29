#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "scanner.h"
#include "object.h"

static Token current, previous;
static bool errorFlag;
static Chunk *out;

static void advanceToken(void)
{
  previous = current;
  current = scanToken();
  if (current.type == TOKEN_ERROR)
  {
    fprintf(stderr, "[line %d] %.*s\n", current.line,
            current.length, current.start);
    errorFlag = true;
  }
}

static void consume(TokenType type, const char *message)
{
  if (current.type == type) { advanceToken(); return; }
  fprintf(stderr, "[line %d] Error: %s\n", current.line, message);
  errorFlag = true;
}

static void emitByte(uint8_t byte) { writeChunk(out, byte, previous.line); }
static void emitBytes(uint8_t a, uint8_t b) { emitByte(a); emitByte(b); }

static int emitJump(uint8_t instruction)
{
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return out->count - 2;
}

static void patchJump(int offset)
{
  int jump = out->count - offset - 2;
  if (jump > UINT16_MAX)
  {
    fprintf(stderr, "Too much code to jump over.\n");
    errorFlag = true;
  }
  out->code[offset] = (uint8_t)((jump >> 8) & 0xff);
  out->code[offset + 1] = (uint8_t)(jump & 0xff);
}

static int precedence(TokenType type)
{
  switch (type)
  {
  case TOKEN_QUESTION: return 1;
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL: return 2;
  case TOKEN_PLUS:
  case TOKEN_MINUS: return 3;
  case TOKEN_STAR:
  case TOKEN_SLASH: return 4;
  default: return 0;
  }
}

static void expression(int minimumPrecedence);

static void primary(void)
{
  switch (current.type)
  {
  case TOKEN_NUMBER:
  {
    double number = strtod(current.start, NULL);
    advanceToken();
    int constant = addConstant(out, NUMBER_VAL(number));
    emitBytes(OP_CONSTANT, (uint8_t)constant);
    return;
  }
  case TOKEN_STRING:
  {
    advanceToken();
    int length = previous.length - 2;
    ObjString *string = copyString(previous.start + 1, length);
    int constant = addConstant(out, OBJ_VAL(string));
    emitBytes(OP_CONSTANT, (uint8_t)constant);
    return;
  }
  case TOKEN_TRUE: advanceToken(); emitByte(OP_TRUE); return;
  case TOKEN_FALSE: advanceToken(); emitByte(OP_FALSE); return;
  case TOKEN_NIL: advanceToken(); emitByte(OP_NIL); return;
  case TOKEN_LEFT_PAREN:
    advanceToken(); expression(0);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    return;
  case TOKEN_MINUS:
    advanceToken(); expression(5); emitByte(OP_NEGATE); return;
  case TOKEN_BANG:
    advanceToken(); expression(5); emitByte(OP_NOT); return;
  default:
    fprintf(stderr, "[line %d] Expect expression.\n", current.line);
    errorFlag = true;
  }
}

static void conditional(void)
{
  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  expression(0);
  consume(TOKEN_COLON, "Expect ':' after then expression.");

  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);
  expression(1);
  patchJump(elseJump);
}

static void expression(int minimumPrecedence)
{
  primary();
  while (precedence(current.type) >= minimumPrecedence &&
         precedence(current.type) > 0)
  {
    TokenType operatorType = current.type;
    int operatorPrecedence = precedence(operatorType);
    advanceToken();

    if (operatorType == TOKEN_QUESTION)
    {
      conditional();
      continue;
    }

    expression(operatorPrecedence + 1);
    switch (operatorType)
    {
    case TOKEN_PLUS: emitByte(OP_ADD); break;
    case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
    case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
    case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_GREATER: emitByte(OP_GREATER); break;
    case TOKEN_LESS: emitByte(OP_LESS); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
    default: break;
    }
  }
}

bool compile(const char *source, Chunk *chunk)
{
  initScanner(source);
  out = chunk;
  errorFlag = false;
  current = scanToken();
  expression(0);
  consume(TOKEN_EOF, "Expect end of expression.");
  emitByte(OP_RETURN);
  return !errorFlag;
}
