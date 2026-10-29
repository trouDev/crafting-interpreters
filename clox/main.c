#include <stdio.h>
#include "compiler.h"
#include "vm.h"
int main(void)
{
    char source[4096];
    initVM();
    while (true)
    {
        printf("> ");
        if (!fgets(source, sizeof source, stdin)) break;
        Chunk chunk;
        initChunk(&chunk);
        if (compile(source, &chunk)) interpret(&chunk);
        freeChunk(&chunk);
    }
    freeVM();
    return 0;
}
