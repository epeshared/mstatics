#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

uint64_t Upper=10*1024*1024;
int Lower=1;

int exe() {    
    size_t i= (rand() % (Upper - Lower + 1)) + Lower;
    char * buffer;
    printf("exe malloc size %d\n", i);

    buffer = (char*) malloc (i+1);
    if (buffer==NULL) {
        printf("can not malloc size %d\n", i);
        exit (1);
    }

    // int n;
    // for (n=0; n<i; n++)
    //     buffer[n]=rand()%26+'a';
    // buffer[i]='\0';

  //printf ("Random string: %s\n",buffer);
  free (buffer);
  return 0;
}

int main() {
    for (int i = 0;i < 20; i++) {
        exe();
    }
}