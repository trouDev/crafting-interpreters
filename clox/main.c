#include <stdio.h>
#include "compiler.h"
int main(void){char source[4096];printf("Enter Lox source: ");if(!fgets(source,sizeof source,stdin))return 0;Chunk c;initChunk(&c);compile(source,&c);freeChunk(&c);return 0;}
