#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include "mstatics.h"

static void* (*real_malloc)(size_t)=NULL;

static void mtrace_init(void) {
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
}


void *malloc(size_t size) {
    if(real_malloc==NULL) {
        mtrace_init();
    }
    
    void *p = NULL;
    fprintf(stderr, "malloc(%d)\n", size);
    p = real_malloc(size);
    //fprintf(stderr, "%p\n", p);
    return p;
}

void *malloc_internal(size_t size, char const * caller_name ) {
    printf( "malloc was called from %s\n", caller_name );
    malloc(size);
}

#define malloc(size) malloc_internal(size, __func__)

