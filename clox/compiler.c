#include <stdio.h>
#include "compiler.h"
#include "scanner.h"
bool compile(const char* source,Chunk* chunk){(void)chunk;initScanner(source);Token t;do{t=scanToken();printf("line %d: %d '%.*s'\n",t.line,t.type,t.length,t.start);}while(t.type!=TOKEN_EOF);return true;}
