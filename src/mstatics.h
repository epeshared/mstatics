#define _GNU_SOURCE 
#include <dlfcn.h>

#include <stdlib.h>
#include <stdio.h>


static void* (*r_malloc)(size_t) = NULL;

void initialize() {
    r_malloc = dlsym(RTLD_NEXT, "malloc");
}

void *malloc_inner(size_t size, char const * caller_name ) {
    printf( "a was called from %s\n", caller_name );
    malloc(size);
}

#define malloc(size) malloc_inner(size, __func__)

/* int main() {
    int i=100;
    char * buffer;

    buffer = (char*) malloc (i+1);
    if (buffer==NULL) exit (1);
} */

