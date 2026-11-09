#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "scanner.h"
#include "object.h"

#define UINT8_COUNT 256

typedef struct { Token name; int depth; } Local;
static Token current, previous;
static bool errorFlag;
static Chunk *out;
static Local locals[UINT8_COUNT];
static int localCount;
static int scopeDepth;

static void advanceToken(void)
{
  previous = current;
  current = scanToken();
  if (current.type == TOKEN_ERROR) errorFlag = true;
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
  if (jump > UINT16_MAX) errorFlag = true;
  out->code[offset] = (uint8_t)((jump >> 8) & 0xff);
  out->code[offset + 1] = (uint8_t)(jump & 0xff);
}

static void emitLoop(int loopStart)
{
  emitByte(OP_LOOP);
  int offset = out->count - loopStart + 2;
  if (offset > UINT16_MAX) errorFlag = true;
  emitByte((uint8_t)((offset >> 8) & 0xff));
  emitByte((uint8_t)(offset & 0xff));
}
static int makeConstant(Value value) { return addConstant(out, value); }
static void emitConstant(Value value) { emitBytes(OP_CONSTANT, (uint8_t)makeConstant(value)); }

static uint8_t identifierConstant(Token *name)
{
  return (uint8_t)makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(const Token *a, const Token *b)
{
  return a->length == b->length &&
         memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Token *name)
{
  for (int i = localCount - 1; i >= 0; i--)
  {
    if (identifiersEqual(name, &locals[i].name)) return i;
  }
  return -1;
}

static void addLocal(Token name)
{
  if (localCount == UINT8_COUNT) { errorFlag = true; return; }
  locals[localCount].name = name;
  locals[localCount].depth = -1;
  localCount++;
}

static void markInitialized(void)
{
  if (scopeDepth == 0) return;
  locals[localCount - 1].depth = scopeDepth;
}

static void declareVariable(Token name)
{
  if (scopeDepth == 0) return;
  addLocal(name);
}

static void namedVariable(Token name)
{
  int local = resolveLocal(&name);
  if (local != -1) emitBytes(OP_GET_LOCAL, (uint8_t)local);
  else emitBytes(OP_GET_GLOBAL, identifierConstant(&name));
}

static void expression(int minimum);

static int precedence(TokenType type)
{
  switch (type)
  {
  case TOKEN_EQUAL_EQUAL: case TOKEN_BANG_EQUAL:
  case TOKEN_GREATER: case TOKEN_GREATER_EQUAL:
  case TOKEN_LESS: case TOKEN_LESS_EQUAL: return 1;
  case TOKEN_PLUS: case TOKEN_MINUS: return 2;
  case TOKEN_STAR: case TOKEN_SLASH: return 3;
  default: return 0;
  }
}

static void primary(void)
{
  switch (current.type)
  {
  case TOKEN_NUMBER: emitConstant(NUMBER_VAL(strtod(current.start, NULL))); advanceToken(); return;
  case TOKEN_STRING: advanceToken(); emitConstant(OBJ_VAL(copyString(previous.start + 1, previous.length - 2))); return;
  case TOKEN_TRUE: advanceToken(); emitByte(OP_TRUE); return;
  case TOKEN_FALSE: advanceToken(); emitByte(OP_FALSE); return;
  case TOKEN_NIL: advanceToken(); emitByte(OP_NIL); return;
  case TOKEN_IDENTIFIER: { Token name = current; advanceToken(); namedVariable(name); return; }
  case TOKEN_LEFT_PAREN: advanceToken(); expression(0); consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression."); return;
  case TOKEN_MINUS: advanceToken(); expression(4); emitByte(OP_NEGATE); return;
  case TOKEN_BANG: advanceToken(); expression(4); emitByte(OP_NOT); return;
  default: fprintf(stderr, "[line %d] Expect expression.\n", current.line); errorFlag = true;
  }
}

static void expression(int minimum)
{
  primary();
  while (precedence(current.type) >= minimum && precedence(current.type) > 0)
  {
    TokenType op = current.type;
    int p = precedence(op);
    advanceToken();
    expression(p + 1);
    switch (op)
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

static void beginScope(void) { scopeDepth++; }

static void endScope(void)
{
  scopeDepth--;
  while (localCount > 0 && locals[localCount - 1].depth > scopeDepth)
  {
    emitByte(OP_POP);
    localCount--;
  }
}

static void declaration(void);
static void statement(void);
static void varDeclaration(void);

static void block(void)
{
  beginScope();
  while (current.type != TOKEN_RIGHT_BRACE && current.type != TOKEN_EOF)
    declaration();
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
  endScope();
}

static void ifStatement(void)
{
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression(0);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();

  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP);
  if (current.type == TOKEN_ELSE) { advanceToken(); statement(); }
  patchJump(elseJump);
}

static void whileStatement(void)
{
  int loopStart = out->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(0);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);
  patchJump(exitJump);
  emitByte(OP_POP);
}

static void forStatement(void)
{
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (current.type == TOKEN_SEMICOLON) advanceToken();
  else if (current.type == TOKEN_VAR) { advanceToken(); varDeclaration(); }
  else { expression(0); consume(TOKEN_SEMICOLON, "Expect ';'."); emitByte(OP_POP); }

  int loopStart = out->count;
  int exitJump = -1;
  if (current.type != TOKEN_SEMICOLON)
  {
    expression(0);
    consume(TOKEN_SEMICOLON, "Expect ';'.");
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }
  else advanceToken();

  if (current.type != TOKEN_RIGHT_PAREN)
  {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = out->count;
    expression(0);
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }
  else advanceToken();

  statement();
  emitLoop(loopStart);
  if (exitJump != -1) { patchJump(exitJump); emitByte(OP_POP); }
  endScope();
}

static void varDeclaration(void)
{
  consume(TOKEN_IDENTIFIER, "Expect variable name.");
  Token name = previous;
  declareVariable(name);
  if (current.type == TOKEN_EQUAL) { advanceToken(); expression(0); }
  else emitByte(OP_NIL);
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  if (scopeDepth > 0) markInitialized();
  else emitBytes(OP_DEFINE_GLOBAL, identifierConstant(&name));
}

static void statement(void)
{
  if (current.type == TOKEN_PRINT)
  {
    advanceToken(); expression(0); consume(TOKEN_SEMICOLON, "Expect ';' after value."); emitByte(OP_PRINT);
  }
  else if (current.type == TOKEN_LEFT_BRACE)
  {
    advanceToken(); block();
  }
  else if (current.type == TOKEN_IF) { advanceToken(); ifStatement(); }
  else if (current.type == TOKEN_WHILE) { advanceToken(); whileStatement(); }
  else if (current.type == TOKEN_FOR) { advanceToken(); forStatement(); }
  else
  {
    expression(0); consume(TOKEN_SEMICOLON, "Expect ';' after expression."); emitByte(OP_POP);
  }
}

static void declaration(void)
{
  if (current.type == TOKEN_VAR) { advanceToken(); varDeclaration(); }
  else statement();
}

bool compile(const char *source, Chunk *chunk)
{
  initScanner(source); out = chunk; errorFlag = false;
  localCount = 0; scopeDepth = 0; current = scanToken();
  while (current.type != TOKEN_EOF) declaration();
  emitByte(OP_NIL); emitByte(OP_RETURN);
  return !errorFlag;
}
