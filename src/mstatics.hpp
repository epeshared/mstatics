#ifndef _MSTATICS_H_
#define _MSTATICS_H_

#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>

#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif

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
#define ENABLE_TRACE 1

static int init_flush_func();

//static const char* FLUSH_INTERVAL = "FLUSH_INTERVAL";

static std::string malloc_latency_file_name;
static FILE *malloc_latency_file = NULL; 
static std::string malloc_interval_file_name;
static FILE *malloc_interval_file = NULL;

static std::string memset_latency_file_name;
static FILE *memset_latency_file = NULL; 
static std::string memset_interval_file_name;
static FILE *memset_interval_file = NULL;

static std::string memmove_latency_file_name;
static FILE *memmove_latency_file = NULL; 
static std::string memmove_interval_file_name;
static FILE *memmove_interval_file = NULL;

static std::string memcpy_latency_file_name;
static FILE *memcpy_latency_file = NULL; 
static std::string memcpy_interval_file_name;
static FILE *memcpy_interval_file = NULL;

static std::string function_trace_file_name;
static FILE *function_trace_file = NULL; 

static std::string out_dir;

std::string pid;

static int _ignore = init_flush_func();

static const char* MSTATICS_OUT_DIR = "MSTATICS_OUT_DIR";
static const char* TIMER_TO_LOG = "TIMER_TO_LOG";

int start_timer();
void* time_to_write_file(void *param) ;

#endif