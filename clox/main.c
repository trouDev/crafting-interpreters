#include <stdio.h>
#include "compiler.h"
#include "vm.h"
int main(void){char source[4096];printf("Enter Lox expression: ");if(!fgets(source,sizeof source,stdin))return 0;Chunk c;initChunk(&c);if(compile(source,&c)){initVM();interpret(&c);freeVM();}freeChunk(&c);return 0;}
