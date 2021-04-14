#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

uint64_t Upper=10*1024*1024;
int Lower=1;

int exe() {    
    size_t size= (rand() % (Upper - Lower + 1)) + Lower;
    char* buffer1;
    printf("exe malloc buffer1 size %d\n", size);

    buffer1 = (char*) malloc (size+1);
    if (buffer1==NULL) {
        printf("can not malloc size %d\n", size);
        exit (1);
    }

    printf("exe memset1\n");

    memset(buffer1, rand()%26+'a', size);

    char* buffer2;
    printf("exe malloc buffer2 size %d\n", size);

    buffer2 = (char*) malloc (size+1);
    if (buffer2==NULL) {
        printf("can not malloc size %d\n", size);
        exit (1);
    }

    printf("exe memset2\n");

    memset(buffer2, rand()%26+'b', size);    

    char* p = memmove(buffer1, buffer2, size);
    printf("%d\n",p[0]);
    // int n;
    // for (n=0; n<10; n++)
    //      printf("%s",p[n]);
    // printf("\n");

    // int n;
    // for (n=0; n<i; n++)
    //     buffer[n]=rand()%26+'a';
    // buffer[i]='\0';

    //printf ("Random string: %s\n",buffer);
    free (buffer1);
    free (buffer2);
    return 0;
}

int main() {
    for (int i = 0;i < 20; i++) {
        exe();
    }
}