#ifndef _MSTATICS_H_
#define _MSTATICS_H_

#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/mman.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

static const char* MSTATICS_OUT_DIR = "MSTATICS_OUT_DIR";
static FILE *log_file = NULL;

int initialize_log() {
    printf("initialize log\n");
    char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);
    printf("tmp_out_dir: %s\n", tmp_out_dir);
    printf("initialize log 2\n");
    char out_dir[128];
    char* default_dir = "./";
    if (tmp_out_dir != NULL) {
      printf("initialize log 2.1\n");
      strcpy(out_dir, tmp_out_dir);
    } else {
      printf("initialize log 2.2\n");
      strcpy(out_dir, default_dir);
    }

    printf("initialize log 3\n");

    char rel_log_file_name[256];
    char* _log_file_name = "//mstatics.log";
    strcpy(rel_log_file_name, out_dir);
    strcat(rel_log_file_name, _log_file_name);
    printf("rel_log_file_name: %s\n", rel_log_file_name);
    printf("initialize log 4\n");
    log_file = fopen(rel_log_file_name,"a+");
    printf("initialize log 5\n");
    if (log_file== NULL) {
        printf("Error opening log file!\n" ,"");
        exit(1);
    }       
}

static int _ignore_log = initialize_log();

#define log_info(fmt, ...) \
    do {fprintf(log_file, "[THREAD:%d INFO] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)

#define log_error(fmt, ...) \
    do {fprintf(log_file, "[THREAD:%d ERROR] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)

#define DEBUG 0
#define log_log(fmt, ...) \
    do { if (DEBUG) fprintf(log_file, "[THREAD:%d] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)

#define LOG_INIT 1
#ifdef LOG_INIT
#define DEBUG_INIT(fmt, ...) \
    do { if (LOG_INIT) fprintf(log_file, "[INIT][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif         

#define LOG_MALLOC 0
#ifdef LOG_MALLOC
#define DEBUG_MALLOC(fmt, ...) \
    do { if (LOG_MALLOC) {fprintf(log_file, "[MALLOC][THREAD:%d] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); }} while (0)      
#endif 

#define LOG_FILE 1
#ifdef LOG_FILE
#define DEBUG_FILE(fmt, ...) \
    do { if (LOG_FILE) fprintf(log_file, "[FILE][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_TIMER 0
#ifdef LOG_TIMER
#define DEBUG_TIMER(fmt, ...) \
    do { if (LOG_TIMER) fprintf(log_file, "[TIMER][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_TRACE 0
#ifdef LOG_TRACE
#define DEBUG_TRACE(fmt, ...) \
    do { if (LOG_TRACE) fprintf(log_file, "[TRACE][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_MEMSET 0
#ifdef LOG_MEMSET
#define DEBUG_MEMSET(fmt, ...) \
    do { if (LOG_MEMSET) fprintf(log_file, "[MEMSET][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif

#define LOG_MEMMOVE 0
#ifdef LOG_MEMMOVE
#define DEBUG_MEMMOVE(fmt, ...) \
    do { if (LOG_MEMMOVE) fprintf(log_file, "[MEMMOVE][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_MEMCPY 0
#ifdef LOG_MEMCPY
#define DEBUG_MEMCPY(fmt, ...) \
    do { if (LOG_MEMCPY) fprintf(log_file, "[LOG_MEMCPY][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define ENABLE_MALLOC 0
#define ENABLE_TRACE 0
#define MERGE_OUTPUT 1

#define BOOST_BACKTRACE 1
#define GLIBC_BACKTRACE 0

#define MAX_RECORD_NUM 20000

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
    _64K_128K_,
    _128K_256K_,
    _256K_512K_,
    _512K_1M_,
    _1M_2M_,
    _2M_4M_,
    GR_4M ,
    invalid_data_size_type = 1000
}data_size_type;

typedef struct {
  char time_buffer[64];
  char type[64]; //memcpy, memset or memmove
  uint64_t size;
  float latency; // time in us
} memmory_usage_record_t;

typedef struct {
  uint64_t count[GR_4M + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  uint64_t size[GR_4M + 1] =  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  uint64_t latency[GR_4M + 1] =  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};// time in us
} memmory_usage_aggre_record_t;

typedef struct {
  char time_buffer[64];
  char function_stack[1024*12];
  uint64_t size;
} trace_record_t;

typedef struct {
  memmory_usage_record_t record[MAX_RECORD_NUM];
  size_t index;
  pthread_mutex_t mutex;
} memory_usage_t;

typedef struct {
  trace_record_t record[MAX_RECORD_NUM];
  size_t index;
  // bool begin_trace;
  // int enabled_ts[GR_4M + 1];
  pthread_mutex_t mutex;
} trace_data_t;

typedef struct {
  bool init;
  pthread_mutex_t mutex;
} init_data_t;

typedef struct {
  bool start;
  pthread_mutex_t mutex;
} timer_status_t;



static std::string memory_usage_file_name;
static FILE* memory_usage_file = NULL;

static std::string function_trace_file_name;
static FILE *function_trace_file = NULL; 

static std::string out_dir;

static void *(*real_memcpy)(void *, const void *, size_t)=NULL;
static void* (*real_malloc)(size_t)=NULL;
static void *(*real_memset)(void *, int, size_t)=NULL;
static void *(*real_memmove)(void *, const void *, size_t)=NULL;

int initialise_shared();
int initialise_trace_data();
int initialise_init_data();
int initialise_timer();
int initialize();

void* create_shared_memory(size_t size);

bool is_initialized();
int write_to_trace_file_and_clean();
int write_to_memory_usage_file_and_clean();

enum data_size check_data_size(size_t size);

int start_timer();
void* time_to_write_file(void *param);

static uint64_t malloc_times = 0;
static uint64_t memset_times = 0;
static uint64_t memmove_times = 0;
static uint64_t memcpy_times = 0;
static uint64_t triger = 1000;
static int finished_init = 0;
static int file_is_opened = 0;
static bool timer_is_activated = false;
thread_local int entry_local_func = 0;

static const char* TIMER_TO_LOG = "TIMER_TO_LOG";
static const char* BEGINE_TO_TRACE = "BEGINE_TO_TRACE";

// The tracing size vriable should be an array to indicate which size sould be traced, 
// the array is 0-1 value for {"1_64", "65_128", "129_256", "257_512", "513_1K", "1K_2K", "2K_4K","4K_8K", "8K_16K", "16K_32K", "32K_64K", "64K_128K", "128K_256K", "256K_512K", "512K_1M", "1M_2M", "2M_4M", ">4M"}
//eg. 
// "1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1" indicate _1_64_ and >4M size should be traced
static const char* TRACING_SIZE = "TRACING_SIZE";

static memory_usage_t* memory_usage_data = NULL;
static int _ignore_shared = initialise_shared();

static trace_data_t* trace_record = NULL;
static int _ignore_trace = initialise_trace_data();

static init_data_t* init_data = NULL;
static int _ignore_init = initialise_init_data();

// static timer_status_t* timer_s = NULL;
// static int _ignore_timer = initialise_timer();

static int _ignore = initialize();

#endif