#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>   // for gettimeofday()
#include <unistd.h>     // for sleep()
#include <signal.h>
#include "mstatics.h"

/********************** log file triger ******************/

static uint64_t malloc_times = 0;
static uint64_t triger = 10;
static int file_is_opened = 0;

/********************** log ******************************/
static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char *log_model_strings[] = {
    "malloc"
};

#define DEBUG 0
#define log_log(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)

#define LOG_MALLOC 0
#ifdef LOG_MALLOC
#define DEBUG_MALLOC(fmt, ...) \
    do { if (LOG_MALLOC) fprintf(stderr, "[MALLOC] %s:%d:%s(): " fmt, __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_FILE 0
#ifdef LOG_FILE
#define DEBUG_FILE(fmt, ...) \
    do { if (LOG_FILE) fprintf(stderr, "[FILE] %s:%d:%s(): " fmt, __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

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
    uint64_t value;
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
    } else {
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

//static const char* FLUSH_INTERVAL = "FLUSH_INTERVAL";
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
        DEBUG_FILE("malloc_latency_file_name: %s\n", malloc_latency_file_name);     

        tmp_out_dir = malloc_interval_file_name;
        malloc_interval_file_name = (char*) real_malloc(strlen(out_dir) + strlen(tmp_out_dir));
        sprintf(malloc_interval_file_name, "%s%s", out_dir, tmp_out_dir);        
        DEBUG_FILE("malloc_latency_file_name: %s\n", malloc_interval_file_name);        

        malloc_latency_file = fopen(malloc_latency_file_name,"w");
        if (malloc_latency_file== NULL) {
            DEBUG_FILE("Error opening malloc_latency_file file!\n" ,"");
            exit(1);
        }

        malloc_interval_file = fopen(malloc_interval_file_name,"w");
        if (malloc_interval_file== NULL) {
            DEBUG_FILE("Error opening malloc_interval_file file!\n", "");
            exit(1);
        }
    }    
}


static void malloc_init(void) {
    DEBUG_MALLOC("malloc init\n", "");
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

static void put_to_time_list(uint64_t time, time_list* list) {
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
    DEBUG_MALLOC("%dus ", _node->value);
    if (_node->next == NULL) {
        DEBUG_MALLOC("\n", "");
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
        sprintf(value_str, "%d ", _node->value);
        size_t value_str_len = strlen(value_str);

        char* last_list_string = list_string;
        DEBUG_MALLOC("list_string_len: %d", list_string_len);
        DEBUG_MALLOC("value_str_len: %d", value_str_len);
        list_string = (char *)real_malloc(list_string_len + value_str_len + 1);
        list_string_len += value_str_len;
        
        if (last_list_string != NULL) {
            sprintf(list_string, "%s%s", last_list_string, value_str);
            //free(last_list_string);  
        } else {
            sprintf(list_string, "%s", value_str);            
        }

         _node = _node->next;
    }

    return list_string;
}

void sig_term_handler(int signum, siginfo_t *info, void *ptr) {
    fprintf(stderr, "sig_term_handler%d\n", signum);
    DEBUG_FILE("Caught signum %d\n", signum);
    exit(signum);
}

void catch_sigterm() {
    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
}

void handle_sigint(int sig) {
    fprintf(stderr, "Caught singal %d\n", sig);
    DEBUG_FILE("Caught singal %d\n", sig);
    exit(sig);
}

void malloc_statics_to_file() {
    DEBUG_MALLOC("************************** to file **************************\n", "");
    char* malloc_statics_string = NULL;
    size_t malloc_statics_string_len = 0;
    file_is_opened = 1;
    malloc_latency_file = fopen(malloc_latency_file_name,"w");
    malloc_interval_file = fopen(malloc_interval_file_name,"w");    
    for (int i = 0;i <= GR_4M; i++) {
        char* latency_statics_string = NULL;
        char* interval_statics_string = NULL;

        malloc_statics_type static_item = malloc_statics[i];        

        char* malloc_size_str = data_size_str[i];
        size_t malloc_size_str_len = strlen(malloc_size_str) + 1; // plush one bit for space char
        DEBUG_MALLOC("---->malloc_size_str:%s\n", malloc_size_str);
                
        if (static_item.count != 0) {
            char count_str[21];
            sprintf(count_str, "%d", static_item.count);
            size_t count_str_len = strlen(count_str) + 1; // plush one bit for space char
            DEBUG_MALLOC("count_str:%s", count_str);

            char* latency_str = time_list_to_string(&(malloc_statics[i].latency_list));
            size_t latency_str_len = strlen(latency_str) + 1; // plush one bit for /n char
            DEBUG_MALLOC("latency_str:%s\n", latency_str);
            latency_statics_string = (char *)real_malloc(malloc_size_str_len + count_str_len + latency_str_len + 1);
            sprintf(latency_statics_string, "%s %s %s\n", malloc_size_str, count_str, latency_str);
            fprintf(malloc_latency_file, "%s", latency_statics_string);            
            free(latency_str);
            free(latency_statics_string);
            latency_statics_string = NULL;
            latency_str = NULL;

            char* interval_str = time_list_to_string(&(malloc_statics[i].interval_list));
            if (interval_str != NULL) {
                size_t interval_str_len = strlen(interval_str) + 3; // plush one bit for /n char
                DEBUG_MALLOC("interval_str:%s\n", interval_str);
                interval_statics_string = (char*)real_malloc(malloc_size_str_len + count_str_len + interval_str_len + 1);
                sprintf(interval_statics_string, "%s %s %s\n", data_size_str[i], count_str, interval_str);
                fprintf(malloc_interval_file, "%s", interval_statics_string);
                free(interval_str);                
                free(interval_statics_string);
                interval_str = NULL;
                interval_statics_string = NULL;
            }                     
        }        
    }
    
    fclose(malloc_latency_file);
    fclose(malloc_interval_file);
    file_is_opened = 0;
}

void *malloc(size_t size) {
    DEBUG_MALLOC("***********************malloc***************************************\n", "");
    if(real_malloc==NULL) {      
        malloc_init();
        DEBUG_FILE("init exit function\n", "");
        atexit(malloc_statics_to_file); 
        // signal(SIGTERM, handle_sigint);
        // signal(SIGILL, handle_sigint);
        // signal(SIGINT, handle_sigint);
        // signal(SIGABRT, handle_sigint);
        // signal(SIGQUIT, handle_sigint);
        // signal(SIGKILL, handle_sigint);
        // signal(SIGSTOP, handle_sigint);
        // catch_sigterm();       
    }

    if (file_is_opened) {
        return real_malloc(size);
    }

    DEBUG_MALLOC("malloc(%d)\n", size);

    enum data_size ds = check_data_size(size);
    DEBUG_MALLOC("data_size(%d)\n", ds);
    malloc_statics[ds].count = malloc_statics[ds].count + 1;

    DEBUG_MALLOC("malloc %s count(%d)\n", data_size_str[ds], malloc_statics[ds].count);
    
    void *p = NULL;
    
    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);
    p = real_malloc(size);
    // stop timer
    gettimeofday(&t2, NULL);

    elapsedTime = (t2.tv_sec - t1.tv_sec);
    elapsedTime += (t2.tv_usec - t1.tv_usec);      
    DEBUG_MALLOC("elapsedTime: %f us.\n", elapsedTime);

    put_to_time_list(elapsedTime, &(malloc_statics[ds].latency_list));
    //DEBUG_MALLOC("latency: \n", "");print_time_list( &(malloc_statics[ds].latency_list));

    if (malloc_statics[ds].count == 1) { // it is called at the first time, no interval is record
        malloc_statics[ds].last_called_time = t2;
    } else {// it is not called at the first time, caculate the interval from last called time
        uint64_t interval = (t1.tv_sec - malloc_statics[ds].last_called_time.tv_sec);
        interval += (t1.tv_usec -  malloc_statics[ds].last_called_time.tv_usec);
        if (interval > 60 * 1000 * 1000) {
            interval = 60 * 1000 * 1000;
        }
        put_to_time_list(interval, &(malloc_statics[ds].interval_list));    
        malloc_statics[ds].last_called_time = t1;  
    }
    //DEBUG_MALLOC("interval: \n", "");print_time_list( &(malloc_statics[ds].interval_list));
    //DEBUG_MALLOC("p: %d\n", p);
    malloc_times++;
    DEBUG_MALLOC("malloc_times: %d\n", malloc_times);
    if (malloc_times >= triger) {
        DEBUG_FILE("triger to write file\n", "");
        malloc_statics_to_file();                    
        malloc_times = 0;
    }
    return p;
}

void *malloc_internal(size_t size, char const * caller_name ) {
    DEBUG_MALLOC( "malloc was called from %s\n", caller_name );
    return malloc(size);
}

#define malloc(size) malloc_internal(size, __function__)
