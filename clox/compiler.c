#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "vm.h"

#define UINT8_COUNT 256

typedef struct
{
  Token name;
  int depth;
  bool isConst;
} Local;
static Token current, previous;
static bool errorFlag;
static Chunk *out;
static Local locals[UINT8_COUNT];
static int localCount;
static int scopeDepth;
static int currentLoopStart = -1;
static int currentLoopScopeDepth = 0;
static int currentLoopSwitchDepth = 0;
static int switchDepth = 0;
static Token *constGlobals;
static int constGlobalCount;
static int constGlobalCapacity;

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

static void addLocal(Token name, bool isConst)
{
  if (localCount == UINT8_COUNT) { errorFlag = true; return; }
  locals[localCount].name = name;
  locals[localCount].depth = -1;
  locals[localCount].isConst = isConst;
  localCount++;
}

static void markInitialized(void)
{
  if (scopeDepth == 0) return;
  locals[localCount - 1].depth = scopeDepth;
}

static void declareVariable(Token name, bool isConst)
{
  if (scopeDepth == 0) return;
  addLocal(name, isConst);
}

static bool isConstGlobal(Token *name)
{
  for (int i = 0; i < constGlobalCount; i++)
  {
    if (identifiersEqual(name, &constGlobals[i])) return true;
  }
  return false;
}

static void rememberConstGlobal(Token name)
{
  if (constGlobalCount == constGlobalCapacity)
  {
    constGlobalCapacity = constGlobalCapacity < 8 ? 8 : constGlobalCapacity * 2;
    constGlobals = realloc(constGlobals,
                           sizeof(Token) * constGlobalCapacity);
    if (constGlobals == NULL) exit(1);
  }
  constGlobals[constGlobalCount++] = name;
}

static void namedVariable(Token name, bool assign)
{
  int local = resolveLocal(&name);
  if (local != -1)
  {
    if (assign)
    {
      if (locals[local].isConst)
      {
        fprintf(stderr, "Cannot assign to a const variable '%.*s'.\n",
                name.length, name.start);
        errorFlag = true;
        return;
      }
      emitBytes(OP_SET_LOCAL, (uint8_t)local);
    }
    else emitBytes(OP_GET_LOCAL, (uint8_t)local);
    return;
  }

  if (assign && isConstGlobal(&name))
  {
    fprintf(stderr, "Cannot assign to a const variable '%.*s'.\n",
            name.length, name.start);
    errorFlag = true;
    return;
  }

  emitBytes(assign ? OP_SET_GLOBAL : OP_GET_GLOBAL, identifierConstant(&name));
}

static void expression(int minimum);

static void emitLoopCleanup(void)
{
  for (int i = localCount - 1;
       i >= 0 && locals[i].depth > currentLoopScopeDepth;
       i--)
  {
    emitByte(OP_POP);
  }

  for (int i = switchDepth; i > currentLoopSwitchDepth; i--)
  {
    emitByte(OP_SWITCH_POP);
  }
}

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
  case TOKEN_IDENTIFIER:
  {
    Token name = current;
    advanceToken();
    if (current.type == TOKEN_EQUAL)
    {
      advanceToken();
      expression(0);
      namedVariable(name, true);
    }
    else namedVariable(name, false);
    return;
  }
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
static void varDeclaration(bool isConst);

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
  int enclosingLoopStart = currentLoopStart;
  int enclosingLoopScopeDepth = currentLoopScopeDepth;
  int enclosingLoopSwitchDepth = currentLoopSwitchDepth;
  currentLoopStart = loopStart;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(0);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  currentLoopScopeDepth = scopeDepth;
  currentLoopSwitchDepth = switchDepth;

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
  emitLoop(loopStart);
  patchJump(exitJump);
  emitByte(OP_POP);
  currentLoopStart = enclosingLoopStart;
  currentLoopScopeDepth = enclosingLoopScopeDepth;
  currentLoopSwitchDepth = enclosingLoopSwitchDepth;
}

static void forStatement(void)
{
  int enclosingLoopStart = currentLoopStart;
  int enclosingLoopScopeDepth = currentLoopScopeDepth;
  int enclosingLoopSwitchDepth = currentLoopSwitchDepth;
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (current.type == TOKEN_SEMICOLON) advanceToken();
  else if (current.type == TOKEN_VAR) { advanceToken(); varDeclaration(false); }
  else if (current.type == TOKEN_CONST) { advanceToken(); varDeclaration(true); }
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

  currentLoopStart = loopStart;
  currentLoopScopeDepth = scopeDepth;
  currentLoopSwitchDepth = switchDepth;
  statement();
  emitLoop(loopStart);
  if (exitJump != -1) { patchJump(exitJump); emitByte(OP_POP); }
  endScope();
  currentLoopStart = enclosingLoopStart;
  currentLoopScopeDepth = enclosingLoopScopeDepth;
  currentLoopSwitchDepth = enclosingLoopSwitchDepth;
}

static void varDeclaration(bool isConst)
{
  consume(TOKEN_IDENTIFIER, "Expect variable name.");
  Token name = previous;
  declareVariable(name, isConst);
  if (current.type == TOKEN_EQUAL) { advanceToken(); expression(0); }
  else if (isConst)
  {
    fprintf(stderr, "A const variable must have an initializer.\n");
    errorFlag = true;
    emitByte(OP_NIL);
  }
  else emitByte(OP_NIL);
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  if (scopeDepth > 0) markInitialized();
  else
  {
    if (isConst) rememberConstGlobal(name);
    emitBytes(OP_DEFINE_GLOBAL, identifierConstant(&name));
  }
}

static bool switchBoundary(void)
{
  return current.type == TOKEN_CASE ||
         current.type == TOKEN_DEFAULT ||
         current.type == TOKEN_RIGHT_BRACE ||
         current.type == TOKEN_EOF;
}

static void switchStatement(void)
{
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
  expression(0);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch value.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");

  emitByte(OP_SWITCH_PUSH);
  switchDepth++;

  int *endJumps = NULL;
  int endJumpCount = 0;
  int endJumpCapacity = 0;
  int defaultJump = -1;
  bool sawDefault = false;

  while (current.type != TOKEN_RIGHT_BRACE && current.type != TOKEN_EOF)
  {
    if (current.type == TOKEN_CASE)
    {
      if (sawDefault)
      {
        fprintf(stderr, "A case cannot appear after default.\n");
        errorFlag = true;
      }
      advanceToken();
      emitByte(OP_SWITCH_GET);
      expression(0);
      consume(TOKEN_COLON, "Expect ':' after case value.");
      emitByte(OP_EQUAL);

      int nextCase = emitJump(OP_JUMP_IF_FALSE);
      emitByte(OP_POP);

      beginScope();
      while (!switchBoundary()) declaration();
      endScope();

      if (endJumpCount == endJumpCapacity)
      {
        endJumpCapacity = endJumpCapacity < 8 ? 8 : endJumpCapacity * 2;
        endJumps = realloc(endJumps, sizeof(int) * endJumpCapacity);
        if (endJumps == NULL) exit(1);
      }
      endJumps[endJumpCount++] = emitJump(OP_JUMP);

      patchJump(nextCase);
      emitByte(OP_POP);
    }
    else if (current.type == TOKEN_DEFAULT)
    {
      if (sawDefault)
      {
        fprintf(stderr, "Only one default case is allowed.\n");
        errorFlag = true;
      }
      sawDefault = true;
      advanceToken();
      consume(TOKEN_COLON, "Expect ':' after default.");

      // The default path is reached only after all case comparisons failed.
      emitByte(OP_SWITCH_POP);
      switchDepth--;
      beginScope();
      while (!switchBoundary()) declaration();
      endScope();
      switchDepth++;
      defaultJump = emitJump(OP_JUMP);
    }
    else
    {
      fprintf(stderr, "Expect 'case' or 'default' in switch.\n");
      errorFlag = true;
      advanceToken();
    }
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch.");

  // A matching case, or a switch without a default, reaches this cleanup.
  for (int i = 0; i < endJumpCount; i++) patchJump(endJumps[i]);
  emitByte(OP_SWITCH_POP);
  if (defaultJump != -1) patchJump(defaultJump);
  free(endJumps);
  switchDepth--;
}

// Novel feature for Challenge 3: unless runs its body when the condition is false.
static void unlessStatement(void)
{
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'unless'.");
  expression(0);
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int falseJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  int endJump = emitJump(OP_JUMP);

  patchJump(falseJump);
  emitByte(OP_POP);
  statement();
  patchJump(endJump);
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
  else if (current.type == TOKEN_SWITCH) { advanceToken(); switchStatement(); }
  else if (current.type == TOKEN_UNLESS) { advanceToken(); unlessStatement(); }
  else if (current.type == TOKEN_CONTINUE)
  {
    advanceToken();
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
    if (currentLoopStart == -1)
    {
      fprintf(stderr, "'continue' must be inside a loop.\n");
      errorFlag = true;
    }
    else
    {
      emitLoopCleanup();
      emitLoop(currentLoopStart);
    }
  }
  else
  {
    expression(0); consume(TOKEN_SEMICOLON, "Expect ';' after expression."); emitByte(OP_POP);
  }
}

static void declaration(void)
{
  if (current.type == TOKEN_VAR) { advanceToken(); varDeclaration(false); }
  else if (current.type == TOKEN_CONST) { advanceToken(); varDeclaration(true); }
  else statement();
}

bool compile(const char *source, Chunk *chunk)
{
  initScanner(source); out = chunk; vm.chunk = chunk; errorFlag = false;
  localCount = 0; scopeDepth = 0; constGlobalCount = 0;
  current = scanToken();
  while (current.type != TOKEN_EOF) declaration();
  emitByte(OP_NIL); emitByte(OP_RETURN);
  return !errorFlag;
}
