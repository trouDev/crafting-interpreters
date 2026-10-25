#include <stdio.h>
#include <stdlib.h>
#include "compiler.h"
#include "scanner.h"
static Token current, previous; static bool errorFlag; static Chunk* out;
static void advanceToken(void){previous=current;current=scanToken();if(current.type==TOKEN_ERROR){fprintf(stderr,"[line %d] %.*s\n",current.line,current.length,current.start);errorFlag=true;}}
static void consume(TokenType type,const char* msg){if(current.type==type){advanceToken();return;}fprintf(stderr,"[line %d] Error: %s\n",current.line,msg);errorFlag=true;}
static void emit(uint8_t b){writeChunk(out,b,previous.line);} static int prec(TokenType t){switch(t){case TOKEN_PLUS:case TOKEN_MINUS:return 1;case TOKEN_STAR:case TOKEN_SLASH:return 2;default:return 0;}}
static void expression(int min);
static void primary(void){if(current.type==TOKEN_NUMBER){double n=strtod(current.start,NULL);advanceToken();int i=addConstant(out,n);writeChunk(out,OP_CONSTANT,previous.line);writeChunk(out,(uint8_t)i,previous.line);return;}if(current.type==TOKEN_LEFT_PAREN){advanceToken();expression(0);consume(TOKEN_RIGHT_PAREN,"Expect ')' after expression.");return;}if(current.type==TOKEN_MINUS){advanceToken();expression(3);emit(OP_NEGATE);return;}fprintf(stderr,"[line %d] Expect expression.\n",current.line);errorFlag=true;}
static void expression(int min){primary();while(prec(current.type)>=min&&prec(current.type)>0){TokenType op=current.type;int p=prec(op);advanceToken();expression(p+1);switch(op){case TOKEN_PLUS:emit(OP_ADD);break;case TOKEN_MINUS:emit(OP_SUBTRACT);break;case TOKEN_STAR:emit(OP_MULTIPLY);break;case TOKEN_SLASH:emit(OP_DIVIDE);break;default:break;}}}
bool compile(const char* source,Chunk* chunk){initScanner(source);out=chunk;errorFlag=false;current=scanToken();expression(0);consume(TOKEN_EOF,"Expect end of expression.");emit(OP_RETURN);return !errorFlag;}
