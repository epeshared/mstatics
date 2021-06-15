#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

uint64_t Upper=10*1024*1024;
int Lower=1;

void *exe(void *ptr) {    
    size_t size= (rand() % (Upper - Lower + 1)) + Lower;
    char* buffer1;
    //printf("exe malloc buffer1 size %d\n", size);

    buffer1 = (char*) malloc (size+1);
    if (buffer1==NULL) {
        printf("can not malloc size %d\n", size);
        exit (1);
    }

    //printf("exe memset1\n");

    memset(buffer1, rand()%26+'a', size);

    char* buffer2;
    //printf("exe malloc buffer2 size %d\n", size);

    buffer2 = (char*) malloc (size+1);
    if (buffer2==NULL) {
        printf("can not malloc size %d\n", size);
        exit (1);
    }

    //printf("exe memset2\n");

    memset(buffer2, rand()%26+'b', size);    

    char* p = (char*)memmove(buffer1, buffer2, size);
    //printf("%d\n",p[0]);
    // int n;
    // for (n=0; n<10; n++)
    //      printf("%s",p[n]);
    // printf("\n");
    p = (char*)memcpy(buffer1, buffer2, size);

    free (buffer1);
    free (buffer2);
}

int main() {

    int ntimes = 20;

    pthread_t *tid = (pthread_t *)malloc( ntimes * sizeof(pthread_t) );

    for(int  i=0; i<ntimes; i++ ) 
        pthread_create( &tid[i], NULL, &exe, NULL );

    sleep(2);

    for(int i=0; i<ntimes; i++ ) 
        pthread_join( tid[i], NULL );

    // for (int i = 0;i < ntimes; i++) {
    //     exe(NULL);
    // }
    sleep(2);
}