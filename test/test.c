#include <stdlib.h>
#include <stdio.h>

int exe() {
    printf("exe malloc\n");
    int i=100;
    char * buffer;

    buffer = (char*) malloc (i+1);
    if (buffer==NULL) exit (1);

    return 0;
}

int main() {
    exe();
}