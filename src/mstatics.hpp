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

#define log_error(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "[THREAD:%d ERROR] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)

#define DEBUG 0
#define log_log(fmt, ...) \
    do { if (DEBUG) fprintf(stderr, "[THREAD:%d] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)

#define LOG_INIT 1
#ifdef LOG_INIT
#define DEBUG_INIT(fmt, ...) \
    do { if (LOG_INIT) fprintf(stderr, "[INIT][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif         

#define LOG_MALLOC 0
#ifdef LOG_MALLOC
#define DEBUG_MALLOC(fmt, ...) \
    do { if (LOG_MALLOC) {fprintf(stderr, "[MALLOC][THREAD:%d] %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); }} while (0)      
#endif 

#define LOG_FILE 0
#ifdef LOG_FILE
#define DEBUG_FILE(fmt, ...) \
    do { if (LOG_FILE) fprintf(stderr, "[FILE][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_TIMER 0
#ifdef LOG_TIMER
#define DEBUG_TIMER(fmt, ...) \
    do { if (LOG_TIMER) fprintf(stderr, "[TIMER][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_TRACE 0
#ifdef LOG_TRACE
#define DEBUG_TRACE(fmt, ...) \
    do { if (LOG_TRACE) fprintf(stderr, "[TRACE][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_MEMSET 0
#ifdef LOG_MEMSET
#define DEBUG_MEMSET(fmt, ...) \
    do { if (LOG_MEMSET) fprintf(stderr, "[MEMSET][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif

#define LOG_MEMMOVE 0
#ifdef LOG_MEMMOVE
#define DEBUG_MEMMOVE(fmt, ...) \
    do { if (LOG_MEMMOVE) fprintf(stderr, "[MEMMOVE][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define LOG_MEMCPY 0
#ifdef LOG_MEMCPY
#define DEBUG_MEMCPY(fmt, ...) \
    do { if (LOG_MEMCPY) fprintf(stderr, "[LOG_MEMCPY][THREAD:%d]  %s:%d:%s(): " fmt, gettid(), __FILE__, \
        __LINE__, __func__, __VA_ARGS__); } while (0)      
#endif 

#define ENABLE_MALLOC 0
#define ENABLE_TRACE 0
#define MERGE_OUTPUT 1

#define BOOST_BACKTRACE 1
#define GLIBC_BACKTRACE 0

#define MAX_RECORD_NUM 5

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

typedef struct {
  char time_buffer[64];
  char type[64]; //memcpy, memset or memmove
  uint64_t size;
  float latency; // time in us
} memmory_usage_record_t;

typedef struct {
  uint64_t count[GR_4M + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  uint64_t size[GR_4M + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  uint64_t latency[GR_4M + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // time in us
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
static uint64_t triger = 100;
static int finished_init = 0;
static int file_is_opened = 0;
static bool timer_is_activated = false;
thread_local int entry_local_func = 0;

static const char* MSTATICS_OUT_DIR = "MSTATICS_OUT_DIR";
static const char* TIMER_TO_LOG = "TIMER_TO_LOG";

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