#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>   // for gettimeofday()
#include <unistd.h>     // for sleep()
#include "mstatics.h"

/********************** define data type *****************/
typedef enum data_size {
    _1_64_,
    _65_128_,
    _129_256_,
    _257_512_,
    _513_1K_,
    _1K_2K_,
    _2K_4K_,
    _4K_8K_,
    _8K_16K_,
    _16K_32K_,
    _32K_64K_,
    _128K_256K_,
    _256K_512K_,
    _512K_1M_,
    _1M_2M_,
    _2M_4M_,
    GR_4M = 16,
    invalid_data_size_type = 1000
}data_size_type;

static char *data_size_str[] = {
    "1-64", "65-128", "129-256", "257-512",
    "513-1K", "1K-2K", "2K-4K","4K-8K", "8K-16K",
    "16K-32K", "32K-64K", "128K-256K", "256K-512K",
    "512K-1M", "1K-2M", "2K-4M", ">4M"
};

struct time_node{
    double value;
    struct time_node* next;
} ;


typedef struct {
    uint64_t size;
    struct time_node* head;
    struct time_node* tail;  
} time_list;


typedef struct  {
    uint64_t count;
    struct timeval last_called_time;
    time_list latency_list;
    time_list interval_list;

} memory_statics;

typedef memory_statics malloc_statics_type;

/********************** util function ***************/

enum data_size check_data_size(size_t size) {
    enum data_size ds = invalid_data_size_type;
    if (size > 1 && size <= 64) {
        ds = _1_64_;
    } else if (size > 65 && size <= 128) {
        ds = _65_128_;
    } else if (size > 129 && size <= 256) {
        ds = _129_256_;
    } else if (size > 257 && size <= 512) {
        ds = _257_512_;
    } else if (size > 513 && size <= 1 * 1024) {
        ds = _513_1K_;
    } else if (size > 1 * 1024 && size <= 2 * 1024) {
        ds = _1K_2K_;
    } else if (size > 2 * 1024 && size <= 4 * 1024) {
        ds = _2K_4K_;
    } else if (size > 4 * 1024 && size <= 8 * 1024) {
        ds = _4K_8K_;
    } else if (size > 8 * 1024 && size <= 16 * 1024) {
        ds = _8K_16K_;
    } else if (size > 16 * 1024 && size <= 32 * 1024) {
        ds = _16K_32K_;
    } else if (size > 32 * 1024 && size <= 64 * 1024) {
        ds = _32K_64K_;
    } else if (size > 128 * 1024 && size <= 256 * 1024) {
        ds = _128K_256K_;
    } else if (size > 256 * 1024 && size <= 512 * 1024) {
        ds = _256K_512K_;
    } else if (size > 512 * 1024 && size <= 1 * 1024 * 1024) {
        ds = _512K_1M_;
    } else if (size > 1 * 1024 * 1024 && size <=  2 * 1024 * 1024) {
        ds = _1M_2M_;
    } else if (size > 2 * 1024 * 1024 && size <= 4 * 1024 * 1024) {
        ds = _2M_4M_;
    } else if (size > 4 * 1024 * 1024) {
        ds = GR_4M;
    } 
    return ds;
}

/********************** malloc **********************/

static malloc_statics_type malloc_statics[GR_4M + 1];

static void* (*real_malloc)(size_t)=NULL;

/********************** file ***********************/
static struct timeval last_flush_time;
static double flush_interval = 1000.0f;
static int flush_init = 0;

static const char* FLUSH_INTERVAL = "FLUSH_INTERVAL";
static const char* MSTATICS_OUT_DIR = "MSTATICS_OUT_DIR";
static char *malloc_latency_file_name = "malloc_latency.data";
static FILE *malloc_latency_file = NULL; 
static char *malloc_interval_file_name = "malloc_interval.data";
static FILE *malloc_interval_file = NULL;

char* out_dir = "./";

void init_flush_func() {    
    if (!flush_init) {
        flush_init = 1;

        gettimeofday(&last_flush_time, NULL);

        char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);
        if (tmp_out_dir != NULL) {
            out_dir = (char*) real_malloc(strlen(tmp_out_dir));
            strcpy(out_dir, tmp_out_dir);
        }
        
        tmp_out_dir = malloc_latency_file_name;
        malloc_latency_file_name = (char*) real_malloc(strlen(out_dir) + strlen(tmp_out_dir));
        sprintf(malloc_latency_file_name, "%s%s", out_dir, tmp_out_dir);        
        fprintf(stderr, "malloc_latency_file_name: %s\n", malloc_latency_file_name);

        tmp_out_dir = malloc_interval_file_name;
        malloc_interval_file_name = (char*) real_malloc(strlen(out_dir) + strlen(tmp_out_dir));
        sprintf(malloc_interval_file_name, "%s%s", out_dir, tmp_out_dir);        
        fprintf(stderr, "malloc_latency_file_name: %s\n", malloc_interval_file_name);        

        char* tmp_flush_interval = getenv (FLUSH_INTERVAL);
        if (tmp_flush_interval != NULL) {
            flush_interval = atof(tmp_flush_interval);
        }
        fprintf(stderr, "flush_interval: %f\n", flush_interval);

        malloc_latency_file = fopen(malloc_latency_file_name,"w");
        if (malloc_latency_file== NULL) {
            fprintf(stderr, "Error opening malloc_latency_file file!\n");
            exit(1);
        }

        malloc_interval_file = fopen(malloc_interval_file_name,"w");
        if (malloc_interval_file== NULL) {
            fprintf(stderr, "Error opening malloc_interval_file file!\n");
            exit(1);
        }
    }    
}


static void malloc_init(void) {
    fprintf(stderr, "malloc init\n");
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    for (int i = _1_64_; i < GR_4M; i++) {
        malloc_statics[i].count = 0;
        malloc_statics[i].latency_list.size = 0;
        malloc_statics[i].latency_list.head = NULL;
        malloc_statics[i].latency_list.tail = NULL;
        malloc_statics[i].interval_list.size = 0;
        malloc_statics[i].interval_list.head = NULL;
        malloc_statics[i].interval_list.tail = NULL;
    }

    init_flush_func();
}

static void put_to_time_list(double time, time_list* list) {
    struct time_node* _node = (struct time_node*)real_malloc(sizeof(struct time_node));
    //memset((void*)_node, 0, sizeof(time_node));
    _node->value = time;
    _node->next = NULL;

    if (list->size == 0) {
        list->head = _node;
        list->tail = _node;
    } else {
        list->tail->next = _node;
        list->tail = _node;
    }

    list->size += 1;

}

typedef void (*iterator_call_back)(struct time_node* _node);

static void print_time_node_value(struct time_node* _node) {
    fprintf(stderr, "%fus ", _node->value);
    if (_node->next == NULL) {
        fprintf(stderr, "\n");
    }
}

static void iterator_time_list(time_list* list, iterator_call_back callback) {
    for (struct time_node* _node = list->head; _node != NULL; ) {
        callback(_node);
         _node = _node->next;
    }
}

static void print_time_list(time_list* list) {
    iterator_time_list(list, print_time_node_value);
}

static char* time_list_to_string(time_list* list) {
    char* list_string = NULL;
    size_t list_string_len = 0;
    for (struct time_node* _node = list->head; _node != NULL; ) {
        char value_str[50];
        sprintf(value_str, "%f.2f ", _node->value);
        size_t value_str_len = strlen(value_str);

        char* last_list_string = list_string;
        list_string = real_malloc(list_string_len + value_str_len);
        
        if (last_list_string != NULL) {
            sprintf(list_string, "%s%s", last_list_string, value_str);
            free(last_list_string);  
        } else {
            sprintf(list_string, "%s", value_str);            
        }

         _node = _node->next;
    }

    return list_string;
}

static char* malloc_statics_to_file() {
    char* malloc_statics_string = NULL;
    size_t malloc_statics_string_len = 0;
    for (int i = 0;i <= GR_4M; i++) {
        char* malloc_statics_string = NULL;
        size_t malloc_statics_string_len = 0;        

        malloc_statics_type static_item = malloc_statics[i];

        char* malloc_size_str = data_size_str[i];
        size_t malloc_size_str_len = strlen(malloc_size_str) + 1; // plush one bit for space char
        
        char count_str[21];
        sprintf(count_str, "%", PRIu64, static_item.count);
        size_t count_str_len = strlen(count_str) + 1; // plush one bit for space char

        char* latency_str = time_list_to_string(&(malloc_statics[i].latency_list));
        size_t latency_str_len = strlen(latency_str) + 1; // plush one bit for /n char

        char* interval_str = time_list_to_string(&(malloc_statics[i].interval_list));
        size_t interval_str_len = strlen(latency_str) + 1; // plush one bit for /n char

        malloc_statics_string = real_malloc(malloc_size_str_len + count_str_len + latency_str_len);

        free(latency_str);
        free(interval_str);
    }
}


void *malloc(size_t size) {
    fprintf(stderr, "***********************malloc***************************************\n");
    if(real_malloc==NULL) {      
        malloc_init();        
    }

    enum data_size ds = check_data_size(size);
    malloc_statics[ds].count = malloc_statics[ds].count + 1;

    fprintf(stderr, "malloc %s count(%d)\n", data_size_str[ds], malloc_statics[ds].count);
    
    void *p = NULL;
    fprintf(stderr, "malloc(%d)\n", size);
    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);
    p = real_malloc(size);
    // stop timer
    gettimeofday(&t2, NULL);

    elapsedTime = (t2.tv_sec - t1.tv_sec);
    elapsedTime += (t2.tv_usec - t1.tv_usec);      
    fprintf(stderr, "elapsedTime: %f us.\n", elapsedTime);

    put_to_time_list(elapsedTime, &(malloc_statics[ds].latency_list));
    fprintf(stderr, "latency: ");print_time_list( &(malloc_statics[ds].latency_list));

    if (malloc_statics[ds].count == 1) { // it is called at the first time, no interval is record
        malloc_statics[ds].last_called_time = t2;
    } else {// it is not called at the first time, caculate the interval from last called time
        double interval = (t1.tv_sec - malloc_statics[ds].last_called_time.tv_sec);
        interval += (t1.tv_usec -  malloc_statics[ds].last_called_time.tv_usec);
        put_to_time_list(interval, &(malloc_statics[ds].interval_list));    
        malloc_statics[ds].last_called_time = t1;  
    }
    fprintf(stderr, "interval: ");print_time_list( &(malloc_statics[ds].interval_list));
    
    return p;
}

void *malloc_internal(size_t size, char const * caller_name ) {
    printf( "malloc was called from %s\n", caller_name );
    return malloc(size);
}

#define malloc(size) malloc_internal(size, __function__)
