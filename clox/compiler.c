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

static int makeConstant(Value value)
{
  return addConstant(out, value);
}

static void emitConstant(Value value)
{
  emitBytes(OP_CONSTANT, (uint8_t)makeConstant(value));
}

static uint8_t identifierConstant(Token *name)
{
  ObjString *string = copyString(name->start, name->length);
  return (uint8_t)makeConstant(OBJ_VAL(string));
}

static void expression(void);

static void primary(void)
{
  switch (current.type)
  {
  case TOKEN_NUMBER:
    emitConstant(NUMBER_VAL(strtod(current.start, NULL)));
    advanceToken();
    return;
  case TOKEN_STRING:
  {
    advanceToken();
    emitConstant(OBJ_VAL(copyString(previous.start + 1,
                                    previous.length - 2)));
    return;
  }
  case TOKEN_TRUE: advanceToken(); emitByte(OP_TRUE); return;
  case TOKEN_FALSE: advanceToken(); emitByte(OP_FALSE); return;
  case TOKEN_NIL: advanceToken(); emitByte(OP_NIL); return;
  case TOKEN_IDENTIFIER:
  {
    Token name = current;
    advanceToken();
    emitBytes(OP_GET_GLOBAL, identifierConstant(&name));
    return;
  }
  case TOKEN_LEFT_PAREN:
    advanceToken(); expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
    return;
  case TOKEN_MINUS:
    advanceToken(); expression(); emitByte(OP_NEGATE); return;
  case TOKEN_BANG:
    advanceToken(); expression(); emitByte(OP_NOT); return;
  default:
    fprintf(stderr, "[line %d] Expect expression.\n", current.line);
    errorFlag = true;
  }
}

static void expression(void)
{
  primary();
}

static void varDeclaration(void)
{
  consume(TOKEN_IDENTIFIER, "Expect variable name.");
  Token name = previous;

  if (current.type == TOKEN_EQUAL)
  {
    advanceToken();
    expression();
  }
  else
  {
    emitByte(OP_NIL);
  }

  if (current.type == TOKEN_SEMICOLON) advanceToken();
  emitBytes(OP_DEFINE_GLOBAL, identifierConstant(&name));
  emitByte(OP_NIL);
}

bool compile(const char *source, Chunk *chunk)
{
  initScanner(source);
  out = chunk;
  errorFlag = false;
  current = scanToken();

  if (current.type == TOKEN_VAR)
  {
    advanceToken();
    varDeclaration();
  }
  else
  {
    expression();
  }

  consume(TOKEN_EOF, "Expect end of expression or declaration.");
  emitByte(OP_RETURN);
  return !errorFlag;
}
