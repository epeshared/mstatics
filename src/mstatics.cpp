#define _GNU_SOURCE
#define __USE_GNU
//#define BOOST_STACKTRACE_USE_ADDR2LINE

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>   // for gettimeofday()
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include "mstatics.hpp"
#include <time.h>
#include <errno.h>
#include <execinfo.h>
#include <threads.h>

#include <mutex>
#include <thread>
#include <fstream>

#include <boost/stacktrace.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <iostream>


//libdwfl
#include <cxxabi.h> // __cxa_demangle
#include <elfutils/libdwfl.h> // Dwfl*
#include <execinfo.h> // backtrace
#include <sstream>
#include <map>


static void* (*real_malloc)(size_t)=NULL;
static void *(*real_memset)(void *, int, size_t)=NULL;

/********************** lock *****************************/

static pthread_mutex_t malloc_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t malloc_file_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memset_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memset_file_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memmove_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memmove_file_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memcpy_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memcpy_file_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;

std::timed_mutex trace_mutex; 
std::mutex function_file_lock;
std::mutex init_lock;

/********************** stack trace **********************/

#define BOOST_BACKTRACE 1
#define GLIBC_BACKTRACE 0


/********************** log file triger ******************/

static uint64_t malloc_times = 0;
static uint64_t memset_times = 0;
static uint64_t memmove_times = 0;
static uint64_t memcpy_times = 0;
static uint64_t triger = 100;
static int finished_init = 0;
static int file_is_opened = 0;
static int timer_is_activated = 0;

/********************** log ******************************/
static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char *log_model_strings[] = {
    "malloc"
};



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

static const char *data_size_str[] = {
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

typedef memory_statics statics_type;

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

int trace_data[GR_4M + 1];
static std::map<std::string, int*> function_trace_data;
thread_local int entry_local_func = 0;
int entried_malloc = 0;

void trace(size_t tracing_size) {
    int lock_times = 0;
    //const std::unique_lock<std::mutex> lock(trace_mutex, std::try_to_lock);
    while (!trace_mutex.try_lock_for(std::chrono::milliseconds(200))) {
        if (lock_times > 5) {
            break;
        }
        lock_times++;
    }
    if (lock_times >= 5) {
        trace_mutex.unlock();
        return;
    }

    entry_local_func += 1;
    std::string call_statck="";
    #if BOOST_BACKTRACE        
        std::stringstream ss;
        ss << boost::stacktrace::stacktrace();
        std::string ss_str;
        int i = 0;
        while(std::getline(ss,ss_str,'\n')){
            
            ss_str = ss_str.substr(0, ss_str.find(" in ", 0));
            size_t pos = ss_str.find("# ");
            ss_str.erase(1,pos);
            while(ss_str.size() && isspace(ss_str.front())) ss_str.erase(ss_str.begin() + (76 - 0x4C));
            while(!ss_str.empty() && isspace(ss_str[ss_str.size() - 1])) ss_str.erase(ss_str.end() - (76 - 0x4B));
            //ss_str = ss_str.substr(ss_str.find("# ", 0), 0);
            if (ss_str.compare("trace(unsigned long)") == 0) {
                continue;
            }

            if (call_statck.compare("") == 0) {
                call_statck = ss_str;
            } else {
                call_statck = call_statck + "<="+ ss_str;
            }            
        }
    #elif LIBDW_BACKTRACE
        std::string calltrace = dwfl_stacktrace();
        //std::cout << calltrace << std::endl;
    #elif GLIBC_BACKTRACE
        char **strings;
        size_t i, size;
        enum Constexpr { MAX_SIZE = 1024 };
        void *array[MAX_SIZE];
        size = backtrace(array, MAX_SIZE);
        strings = backtrace_symbols(array, size);        
        for (i = 0; i < size; i++) {
            std::string bt_str = std::string(strings[i]);
            bt_str = bt_str.substr(0, bt_str.find(" [", 0));
            if (i == 0) {
                continue;
            }

            if (call_statck.compare("") == 0) {
                call_statck = bt_str;
            } else {
                call_statck = call_statck + "<="+ bt_str;
            }         
        }
        //std::cout << call_statck << std::endl;
        free(strings);
    #endif 
    
    auto it = function_trace_data.find(call_statck);
    int size_range = check_data_size(tracing_size);

    if (it != function_trace_data.end()) {
        int* traced_data = it->second;
        traced_data[size_range] = traced_data[size_range] + 1;
    } else {
        if  (real_malloc == NULL) {
            real_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
        }

        int* traced_data = (int*)real_malloc((GR_4M+1)*sizeof(int));

        DEBUG_TRACE("new key %s size_range %d \n", call_statck.c_str(), size_range);
        if (real_memset == NULL) {
            real_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
        }     
        real_memset(traced_data, 0, (GR_4M + 1) *sizeof(int));
        traced_data[size_range] = 1;  
        function_trace_data.insert(std::pair<std::string, int*>(call_statck, traced_data));
        
    }    
    // for (auto const &pair: function_trace_data) {
    //     std::ostringstream os;
    //     int* traced_data = pair.second;
    //     // for (int i = 0; i < GR_4M; i++) {
    //     //     if (i = 0) {
    //     //         os << traced_data[i];
    //     //     } else {
    //     //         os << ",";
    //     //         os << traced_data[i];
    //     //     }
    //     // }

    //     // std::string str(os.str());
    //     std::cout << "{" << pair.first << ": " << traced_data[0] << "," << traced_data[1] << "," << traced_data[2] << "," << traced_data[3] << ","
    //                      << traced_data[4] << "," << traced_data[5] << "," << traced_data[6] << "," << traced_data[7] << ","
    //                      << traced_data[8] << "," << traced_data[9] << "," << traced_data[10] << "," << traced_data[11] << "," 
    //                      << traced_data[12] << "," << traced_data[13] << "," << traced_data[14] << "," << traced_data[15] << "}\n";
    // }
    entry_local_func = entry_local_func - 1;
    trace_mutex.unlock();
}


/********************** malloc **********************/

static statics_type malloc_statics[GR_4M + 1];

/********************** file ***********************/
static struct timeval last_flush_time;
static double flush_interval = 1000.0f;
static int flush_init = 0;

static void malloc_init(void);
static char function_trace_header[] = "function 1-64 65-128 129-256 257-512 513-1K 1K-2K 2K-4K 4K-8K 8K-16K 16K-32K 32K-64K 128K-256K 256K-512K 512K-1M 1K-2M 2K-4M >4M";


static std::string assign_file_name(std::string file_name) {
    char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);        
    if (tmp_out_dir != NULL) {
        out_dir = std::string(tmp_out_dir);
        DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    } else {
        out_dir = "./";
        DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    }
}

void reset_env() {
    //finished_init = 0;
    timer_is_activated = 0;
}

static int init_flush_func() {     
    const std::lock_guard<std::mutex> lock(init_lock);
    entry_local_func += 1;    
    std::string current_pid = std::to_string(getpid());
    //DEBUG_INIT("last pid %s, current_pid %s starting init\n", pid.c_str(), current_pid.c_str()); 
    if (!pid.empty() && pid != current_pid) {
        pid = current_pid;
        DEBUG_INIT("pid %s reset init\n", pid.c_str());
        reset_env();
    } 

    pid = current_pid;

    if (finished_init) {
        entry_local_func--;
        return 0;
    }

    if(real_malloc==NULL) {      
        malloc_init();
    }

    DEBUG_INIT("pid %s starting init\n", pid.c_str());        

    malloc_latency_file_name = "malloc_latency.data";
    malloc_interval_file_name = "malloc_interval.data";

    memset_latency_file_name = "memset_latency.data";
    memset_interval_file_name = "memset_interval.data";

    memmove_latency_file_name = "memmove_latency.data";
    memmove_interval_file_name = "memmove_interval.data";

    memcpy_latency_file_name = "memcpy_latency.data";
    memcpy_interval_file_name = "memcpy_interval.data";

    function_trace_file_name = "function_trace.data"; 

    out_dir = "./";  

    if (!flush_init) {
        flush_init = 1;  

        char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);        
        if (tmp_out_dir != NULL) {
            out_dir = std::string(tmp_out_dir);
            DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
        } else {
            out_dir = "./";
            DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
        }
        
        //std::cout << "DEBUG: init function called process " << ::getpid() << std::endl;                

        gettimeofday(&last_flush_time, NULL);

        char* COUNT_TO_LOG_STR = getenv(TIMER_TO_LOG);
        if (COUNT_TO_LOG_STR != NULL) {
            triger = atoi(COUNT_TO_LOG_STR);            
        }
        DEBUG_INIT("TIMER_TO_LOG is change to %d\n", triger);

        char header[] = "time 1-64 65-128 129-256 257-512 513-1K 1K-2K 2K-4K 4K-8K 8K-16K 16K-32K 32K-64K 128K-256K 256K-512K 512K-1M 1K-2M 2K-4M >4M";        
        
        /******/
        // tmp_out_dir = malloc_latency_file_name;
        // malloc_latency_file_name = (char*) real_malloc(strlen(out_dir) + strlen(tmp_out_dir));
        // sprintf(malloc_latency_file_name, "%s%s", out_dir, tmp_out_dir);

        
        //malloc_latency_file_name = out_dir + pid + "_" + malloc_latency_file_name;
        malloc_latency_file_name = out_dir + malloc_latency_file_name;
        DEBUG_INIT("malloc_latency_file_name: %s\n", malloc_latency_file_name.c_str());     

        //malloc_interval_file_name = out_dir + pid + "_" + malloc_interval_file_name; 
        malloc_interval_file_name = out_dir + malloc_interval_file_name; 
        DEBUG_INIT("malloc_interval_file_name: %s\n", malloc_interval_file_name.c_str());        

        malloc_latency_file = fopen(malloc_latency_file_name.c_str(),"w");
        if (malloc_latency_file== NULL) {
            DEBUG_FILE("Error opening malloc_latency_file file!\n" ,"");
            exit(1);
        }        

        malloc_interval_file = fopen(malloc_interval_file_name.c_str(),"w");
        if (malloc_interval_file== NULL) {
            DEBUG_FILE("Error opening malloc_interval_file file!\n", "");
            exit(1);
        }

        fprintf(malloc_latency_file, "%s\n", header);
        fprintf(malloc_interval_file, "%s\n", header);


        fclose(malloc_latency_file);
        fclose(malloc_interval_file);

        /******/
        //memset_latency_file_name = out_dir + pid + "_" + memset_latency_file_name;     
        memset_latency_file_name = out_dir + memset_latency_file_name;     
        DEBUG_INIT("memset_latency_file_name: %s\n", memset_latency_file_name.c_str());     

        //memset_interval_file_name = out_dir + pid + "_" + memset_interval_file_name;         
        memset_interval_file_name = out_dir + memset_interval_file_name;         
        DEBUG_INIT("memset_intervalfile_name: %s\n", memset_interval_file_name.c_str());        

        memset_latency_file = fopen(memset_latency_file_name.c_str(),"w");
        if (memset_latency_file== NULL) {
            DEBUG_FILE("Error opening memset_latency_file file!\n" ,"");
            exit(1);
        }

        memset_interval_file = fopen(memset_interval_file_name.c_str(),"w");
        if (memset_interval_file== NULL) {
            DEBUG_FILE("Error opening memset_interval_file file!\n", "");
            exit(1);
        }

        fprintf(memset_latency_file, "%s\n", header);
        fprintf(memset_interval_file, "%s\n", header);

        fclose(memset_latency_file);
        fclose(memset_interval_file);               

        //memmove_latency_file_name = out_dir + pid + "_" + memmove_latency_file_name; 
        memmove_latency_file_name = out_dir +  memmove_latency_file_name; 
        DEBUG_INIT("memmove_latency_file_name: %s\n", memmove_latency_file_name.c_str());     

        //memmove_interval_file_name = out_dir + pid + "_" + memmove_interval_file_name;       
        memmove_interval_file_name = out_dir + memmove_interval_file_name;       
        DEBUG_INIT("memmove_interval_file_name: %s\n", memmove_interval_file_name.c_str()); 

        memmove_latency_file = fopen(memmove_latency_file_name.c_str(),"w");
        if (memmove_latency_file== NULL) {
            DEBUG_FILE("Error opening memmove_latency_file file!\n" ,"");
            exit(1);
        }

        memmove_interval_file = fopen(memmove_interval_file_name.c_str(),"w");
        if (memmove_interval_file== NULL) {
            DEBUG_FILE("Error opening memmove_interval_file file!\n", "");
            exit(1);
        }

        fprintf(memmove_latency_file, "%s\n", header);
        fprintf(memmove_interval_file, "%s\n", header);        

        fclose(memmove_latency_file);
        fclose(memmove_interval_file);      

        /****/
        //memcpy_latency_file_name = out_dir + pid + "_" + memcpy_latency_file_name;
        memcpy_latency_file_name = out_dir + memcpy_latency_file_name;
        DEBUG_INIT("memcpy_latency_file_name: %s\n", memcpy_latency_file_name.c_str());     

        //memcpy_interval_file_name = out_dir + pid + "_" + memcpy_interval_file_name;       
        memcpy_interval_file_name = out_dir + memcpy_interval_file_name;       
        DEBUG_INIT("memcpy_interval_file_name: %s\n", memcpy_interval_file_name.c_str()); 

        memcpy_latency_file = fopen(memcpy_latency_file_name.c_str(),"w");
        if (memcpy_latency_file== NULL) {
            DEBUG_FILE("Error opening memcpy_latency_file file!\n" ,"");
            exit(1);
        }

        memcpy_interval_file = fopen(memcpy_interval_file_name.c_str(),"w");
        if (memcpy_interval_file== NULL) {
            DEBUG_FILE("Error opening memcpy_interval_file file!\n", "");
            exit(1);
        }

        fprintf(memcpy_latency_file, "%s\n", header);
        fprintf(memcpy_interval_file, "%s\n", header);        

        fclose(memcpy_latency_file);
        fclose(memcpy_interval_file); 

        /*** trace file name *****/
        //function_trace_file_name = out_dir + pid + "_" + function_trace_file_name;  
        function_trace_file_name = out_dir + function_trace_file_name;  
        DEBUG_INIT("function_trace_file_name: %s\n", function_trace_file_name.c_str());

        function_trace_file = fopen(function_trace_file_name.c_str(),"w");
        if (function_trace_file== NULL) {
            DEBUG_FILE("Error opening function_trace_file file!\n" ,"");
            exit(1);
        }

        fprintf(function_trace_file, "%s\n", function_trace_header);

        //fprintf(function_trace_header, "%s\n", header);  
        fclose(function_trace_file);
        finished_init = 1;
        DEBUG_INIT("finished init\n", "");
    } 

    entry_local_func -= 1; 

    return 0;
}

static void malloc_init(void) {
    DEBUG_MALLOC("malloc init\n", "");
    real_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
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

static uint64_t get_avg_latency(time_list* list) {
    int count = 0;
    uint64_t total_latency = 0;
    for (struct time_node* _node = list->head; _node != NULL; ) {
        count++;
        total_latency += _node->value;
         _node = _node->next;
    }

    log_log("total_latency: %d\n", total_latency);
    log_log("count: %d\n", count);

    uint64_t avg_latency = (count == 0 ? total_latency : (total_latency/count));
    log_log("avg_latency : %d\n", avg_latency);
    return avg_latency;
}


// static char* time_list_to_string(time_list* list) {
//     char* list_string = NULL;
//     size_t list_string_len = 0;
//     for (struct time_node* _node = list->head; _node != NULL; ) {
//         char value_str[50];
//         sprintf(value_str, "%d ", _node->value);
//         size_t value_str_len = strlen(value_str);

//         char* last_list_string = list_string;
//         DEBUG_MALLOC("list_string_len: %d", list_string_len);
//         DEBUG_MALLOC("value_str_len: %d", value_str_len);
//         list_string = (char *)real_malloc(list_string_len + value_str_len + 1);
//         list_string_len += value_str_len;
        
//         if (last_list_string != NULL) {
//             sprintf(list_string, "%s%s", last_list_string, value_str);
//             //free(last_list_string);  
//         } else {
//             sprintf(list_string, "%s", value_str);            
//         }

//          _node = _node->next;
//     }

//     return list_string;
// }

char *trimwhitespace(char *str) {
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

typedef struct {
    FILE* latency_file;
    char* latency_file_name;
    FILE* interval_file;
    char* interval_file_name;
    statics_type* statics;
} timer_data;

void set_static_list_to_zero(time_list* list) {
    for (struct time_node* _node = list->head; _node != NULL; ) {
         _node->value = 0;
    }
}

void free_statics_data(statics_type* static_data) {
    for (int i = _1_64_; i <= GR_4M; i++) {
        (static_data)[i].count = 0;
        (static_data)[i].latency_list.size = 0;
        (static_data)[i].interval_list.size = 0;
        
        //free(static_data[i].latency_list.head);
        //set_static_list_to_zero(&(static_data[i]).latency_list);
        (static_data)[i].latency_list.head = NULL;
        //free(static_data[i].latency_list.tail);
        (static_data)[i].latency_list.tail = NULL;

        //free(static_data[i].interval_list.head);
        //set_static_list_to_zero(&(static_data[i]).interval_list);
        (static_data)[i].interval_list.head = 0;        
        //free(static_data[i].interval_list.tail);
        (static_data)[i].interval_list.tail = NULL;
    }    
}

void function_statics_to_file() {
 
    if(real_malloc==NULL) {      
        malloc_init();
    }

    entry_local_func++;

    DEBUG_TRACE("write to file:%s\n", function_trace_file_name.c_str());

    function_trace_file = fopen(function_trace_file_name.c_str(),"a");

    //fprintf(function_trace_file, "%s\n", function_trace_header);
    
    if (function_trace_data.empty()) {
        entry_local_func--;
        fclose(function_trace_file);
        return;
    }

    for (auto const &pair: function_trace_data) {
        std::ostringstream os;
        int* traced_data = pair.second;
        // for (int i = 0; i < GR_4M; i++) {
        //     if (i = 0) {
        //         os << traced_data[i];
        //     } else {
        //         os << ",";
        //         os << traced_data[i];
        //     }
        // }

        // std::string str(os.str());
        os << pair.first << " " << traced_data[0] << " " << traced_data[1] << " " << traced_data[2] << " " << traced_data[3] << " "
                         << traced_data[4] << " " << traced_data[5] << " " << traced_data[6] << " " << traced_data[7] << " "
                         << traced_data[8] << " " << traced_data[9] << " " << traced_data[10] << " " << traced_data[11] << " " 
                         << traced_data[12] << " " << traced_data[13] << " " << traced_data[14] << " " << traced_data[15] << " " << traced_data[16] << "\n";
        std::string str(os.str());
        DEBUG_TRACE("add line %s",str.c_str());
        fprintf(function_trace_file, "%s", str.c_str());        
    }    
    fclose(function_trace_file);
    function_trace_data.clear();
    entry_local_func--;
}

void statics_to_file(FILE* latency_file, const char* latency_file_name, 
    FILE* interval_file, const char* interval_file_name, statics_type* statics) {

    entry_local_func += 1;
    if(real_malloc==NULL) {      
        malloc_init();
    }  

    DEBUG_FILE("write to file:%s\n", latency_file_name);
    DEBUG_FILE("write to file:%s\n", interval_file_name);
    file_is_opened = 1;

    // time_t timer;
    // char time_buffer[26];
    // struct tm* tm_info;

    // timer = time(NULL);
    // tm_info = localtime(&timer);

    // strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S.%f", tm_info); 

    char fmt[64], time_buffer[64];
    struct timeval  tv;
    struct tm *tm;

    gettimeofday(&tv, NULL);
    if((tm = localtime(&tv.tv_sec)) != NULL) {
        strftime(fmt, sizeof fmt, "%Y-%m-%d-%H:%M:%S.%%06u", tm);
        snprintf(time_buffer, sizeof time_buffer, fmt, tv.tv_usec);
        // printf("'%s'\n", time_buffer); 
    }       
    
    latency_file = fopen(latency_file_name,"a");    
    interval_file = fopen(interval_file_name,"a");    

    char* latency_statics_string = NULL;
    char* interval_statics_string = NULL;
    uint64_t avg_latency_list[GR_4M + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint64_t count_list[GR_4M + 1] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    for (int i = 0;i <= GR_4M ; i++) {
        statics_type static_item = statics[i];        

        const char* size_str = data_size_str[i];
        size_t size_str_len = strlen(size_str) + 1; // plush one bit for space char
        //DEBUG_FILE("---->size_str:%s\n", size_str);
        
        avg_latency_list[i] = get_avg_latency(&(statics[i].latency_list));
        //DEBUG_FILE("avg latency : %d\n", avg_latency_list[i]);

        count_list[i] = static_item.count;
        //DEBUG_FILE("count : %d\n", count_list[i]);     
    }

    char* latency_list_string = NULL;
    size_t latency_list_string_len = 0;
    
    char* count_list_string = NULL;
    size_t count_list_string_len = 0;

    for (int i = 0;i <= GR_4M ; i++) {
        char avg_latency_str[64];
        sprintf(avg_latency_str, "%d", avg_latency_list[i]);
        size_t value_str_len = strlen(avg_latency_str) + 1;

        char* last_list_string = latency_list_string;
        latency_list_string = (char *)real_malloc(latency_list_string_len + value_str_len);
        latency_list_string_len += value_str_len;
        
        if (last_list_string != NULL) {
            sprintf(latency_list_string, "%s %s", last_list_string, avg_latency_str);
            //free(last_list_string);  
        } else {
            sprintf(latency_list_string, "%s", avg_latency_str);            
        }
        // DEBUG_FILE("latency_list_string: %s\n", latency_list_string);

        char count_str[64];
        sprintf(count_str, "%d", count_list[i]);
        //DEBUG_FILE("count_str: %s\n", count_str);
        size_t count_str_len = strlen(count_str) + 1;
        //DEBUG_FILE("count_str_len: %d\n", count_str_len);

        char* last_count_string = count_list_string;
        count_list_string = (char *)real_malloc(count_list_string_len + count_str_len);
        count_list_string_len += count_str_len;
        
        if (last_count_string != NULL) {
            sprintf(count_list_string, "%s %s", last_count_string, count_str);
            //free(last_list_string);  
        } else {
            sprintf(count_list_string, "%s", count_str);            
        }        
    }

    char* latency_line =  (char *)real_malloc(strlen(time_buffer) + 1 + latency_list_string_len);
    //trimwhitespace(latency_list_string);
    sprintf(latency_line, "%s %s", time_buffer, latency_list_string);
    //trimwhitespace(latency_line);
    fprintf(latency_file, "%s\n", latency_line);

    char* count_line =  (char *)real_malloc(strlen(time_buffer) + 1 + count_list_string_len);    
    sprintf(count_line, "%s %s", time_buffer, count_list_string);
    //trimwhitespace(count_line);
    fprintf(interval_file, "%s\n", count_line);

    free_statics_data(statics);  
    
    fclose(latency_file);
    fclose(interval_file);
    file_is_opened = 0;
    entry_local_func -= 1;
}

void malloc_statics_to_file() {
    //pthread_mutex_lock(&malloc_file_lock);
    statics_to_file(malloc_latency_file, malloc_latency_file_name.c_str(), 
        malloc_interval_file, malloc_interval_file_name.c_str(), malloc_statics);
    //pthread_mutex_unlock(&malloc_file_lock);
}

#if ENABLE_MALLOC
void *malloc(size_t size) {
    if (entry_local_func) {
        if (real_malloc == NULL) {
            real_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
        }
        return real_malloc(size);
    }

    DEBUG_MALLOC("will to malloc file:%s\n", malloc_latency_file_name.c_str());
    DEBUG_MALLOC("will to malloc file:%s\n", malloc_interval_file_name.c_str());      
      
    init_flush_func();     
    DEBUG_MALLOC("malloc %d\n", size);   

    //trace(size);      

    pthread_mutex_lock(&malloc_lock);
    if(real_malloc==NULL) {        
        malloc_init();
        //atexit(malloc_statics_to_file); 
        for (int i = _1_64_; i < GR_4M; i++) {
            malloc_statics[i].count = 0;
            malloc_statics[i].latency_list.size = 0;
            malloc_statics[i].latency_list.head = NULL;
            malloc_statics[i].latency_list.tail = NULL;
            malloc_statics[i].interval_list.size = 0;
            malloc_statics[i].interval_list.head = NULL;
            malloc_statics[i].interval_list.tail = NULL;
        }
    }
    // pthread_mutex_unlock(&malloc_lock);

    // start_timer();
    
    // pthread_mutex_lock(&malloc_lock);

    DEBUG_MALLOC("malloc(%d)\n", size);
    enum data_size ds = check_data_size(size);
    DEBUG_MALLOC("data_size(%d)\n", ds);
    malloc_statics[ds].count = malloc_statics[ds].count + 1;
    // pthread_mutex_unlock(&malloc_lock);
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

    // pthread_mutex_lock(&malloc_lock);
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
    // malloc_times++;
    // // pthread_mutex_unlock(&malloc_lock);
    // DEBUG_MALLOC("malloc_times: %d\n", malloc_times);
    // if (malloc_times >= triger) {
    //     DEBUG_FILE("triger to write file\n", "");
    //     malloc_statics_to_file();                    
    //     malloc_times = 0;
    // }
    pthread_mutex_unlock(&malloc_lock);
    DEBUG_MALLOC("write to malloc file:%s\n", malloc_latency_file_name.c_str());
    DEBUG_MALLOC("write to malloc file:%s\n", malloc_interval_file_name.c_str());    
    start_timer();
    DEBUG_MALLOC("after write to malloc file:%s\n", malloc_latency_file_name.c_str());
    DEBUG_MALLOC("after write to malloc file:%s\n", malloc_interval_file_name.c_str());        
    return p;
}

void *malloc_internal(size_t size, char const * caller_name ) {
    DEBUG_MALLOC( "malloc was called from %s\n", caller_name );
    return malloc(size);
}

#define malloc(size) malloc_internal(size, __function__)
#endif
/*************************** memset ******************************************/

 

typedef memory_statics memset_statics_type;
static memset_statics_type memset_statics[GR_4M + 1];

static void memset_init(void) {
    DEBUG_MEMSET("MEMSET init\n", "");
    real_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
    if (NULL == real_memset) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
}

void memset_statics_to_file() {
    //DEBUG_MEMSET("Write to MEMSET file\n", "");
    statics_to_file(memset_latency_file, memset_latency_file_name.c_str(), 
        memset_interval_file, memset_interval_file_name.c_str(), memset_statics);
}

void *memset(void *str, int c, size_t size) {
    if (entry_local_func) {
        if (real_memset == NULL) {
            real_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
        }
        return real_memset(str, c, size);;
    } 
    init_flush_func(); 
    DEBUG_MEMSET("memset\n","");        
    #if ENABLE_TRACE
    trace(size);
    #endif
    
    pthread_mutex_lock(&memset_lock);
    if (real_memset == NULL) {
        memset_init();
        for (int i = _1_64_; i < GR_4M + 1; i++) {
            memset_statics[i].count = 0;
            memset_statics[i].latency_list.size = 0;
            memset_statics[i].latency_list.head = NULL;
            memset_statics[i].latency_list.tail = NULL;
            memset_statics[i].interval_list.size = 0;
            memset_statics[i].interval_list.head = NULL;
            memset_statics[i].interval_list.tail = NULL;
        }
    }
    // pthread_mutex_unlock(&memset_lock);

    // start_timer();

    // pthread_mutex_lock(&memset_lock);    

    DEBUG_MEMSET("memset(%d)\n", size);

    enum data_size ds = check_data_size(size);
    DEBUG_MEMSET("data_size(%d)\n", ds);
    memset_statics[ds].count = memset_statics[ds].count + 1;
    // pthread_mutex_unlock(&memset_lock);

    DEBUG_MEMSET("memset %s count(%d)\n", data_size_str[ds], memset_statics[ds].count);
    
    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);
    void* p = real_memset(str, c, size);
    // stop timer
    gettimeofday(&t2, NULL);

    elapsedTime = (t2.tv_sec - t1.tv_sec);
    elapsedTime += (t2.tv_usec - t1.tv_usec);      
    DEBUG_MEMSET("elapsedTime: %f us.\n", elapsedTime);

    // pthread_mutex_lock(&memset_lock);
    put_to_time_list(elapsedTime, &(memset_statics[ds].latency_list));

    if (memset_statics[ds].count == 1) { // it is called at the first time, no interval is record
        memset_statics[ds].last_called_time = t2;
    } else {// it is not called at the first time, caculate the interval from last called time
        uint64_t interval = (t1.tv_sec - memset_statics[ds].last_called_time.tv_sec);
        interval += (t1.tv_usec -  memset_statics[ds].last_called_time.tv_usec);
        if (interval > 60 * 1000 * 1000) {
            interval = 60 * 1000 * 1000;
        }
        put_to_time_list(interval, &(memset_statics[ds].interval_list));    
        memset_statics[ds].last_called_time = t1;  
    }

    // memset_times++;
    // // pthread_mutex_unlock(&memset_lock);
    // DEBUG_MEMSET("memset_times: %d\n", memset_times);
    // if (memset_times >= triger) {
    //     DEBUG_FILE("triger to write file\n", "");
    //     memset_statics_to_file();                    
    //     memset_times = 0;
    // }
    pthread_mutex_unlock(&memset_lock);
    start_timer();
    return p;    
}

/*************************** memmove ******************************************/


typedef memory_statics memmove_statics_type;
static memmove_statics_type memmove_statics[GR_4M + 1];

static void *(*real_memmove)(void *, const void *, size_t)=NULL;

static void memmove_init(void) {
    DEBUG_MEMMOVE("MEMSET init\n", "");
    real_memmove = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memmove");
    if (NULL == real_memmove) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
}

void memmove_statics_to_file() {
    statics_to_file(memmove_latency_file, memmove_latency_file_name.c_str(), 
        memmove_interval_file, memmove_interval_file_name.c_str(), memmove_statics);
}

void *memmove(void *str1, const void *str2, size_t size) {
    if (entry_local_func) {
        if (real_memmove == NULL) {
           real_memmove = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memmove");
        }
        return real_memmove(str1, str2, size);
    }
    init_flush_func();  
    DEBUG_MEMMOVE("memmove","");
    
    #if ENABLE_TRACE
    trace(size);
    #endif

    pthread_mutex_lock(&memmove_lock);
    if (real_memmove == NULL) {
        memmove_init();
        for (int i = _1_64_; i < GR_4M; i++) {
            memmove_statics[i].count = 0;
            memmove_statics[i].latency_list.size = 0;
            memmove_statics[i].latency_list.head = NULL;
            memmove_statics[i].latency_list.tail = NULL;
            memmove_statics[i].interval_list.size = 0;
            memmove_statics[i].interval_list.head = NULL;
            memmove_statics[i].interval_list.tail = NULL;
        }     
    }
    // pthread_mutex_unlock(&memmove_lock); 
    // start_timer();

    // pthread_mutex_lock(&memmove_lock);     

    DEBUG_MEMMOVE("memmove(%d)\n", size);

    enum data_size ds = check_data_size(size);
    DEBUG_MEMMOVE("data_size(%d)\n", ds);
    memmove_statics[ds].count = memmove_statics[ds].count + 1;
    // pthread_mutex_unlock(&memmove_lock);

    DEBUG_MEMMOVE("memmove %s count(%d)\n", data_size_str[ds], memmove_statics[ds].count);
    
    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);
    void* p = real_memmove(str1, str2, size);
    // stop timer
    gettimeofday(&t2, NULL);

    elapsedTime = (t2.tv_sec - t1.tv_sec);
    elapsedTime += (t2.tv_usec - t1.tv_usec);      
    DEBUG_MEMMOVE("elapsedTime: %f us.\n", elapsedTime);

    // pthread_mutex_lock(&memmove_lock);
    put_to_time_list(elapsedTime, &(memmove_statics[ds].latency_list));

    if (memmove_statics[ds].count == 1) { // it is called at the first time, no interval is record
        memmove_statics[ds].last_called_time = t2;
    } else {// it is not called at the first time, caculate the interval from last called time
        uint64_t interval = (t1.tv_sec - memmove_statics[ds].last_called_time.tv_sec);
        interval += (t1.tv_usec -  memmove_statics[ds].last_called_time.tv_usec);
        if (interval > 60 * 1000 * 1000) {
            interval = 60 * 1000 * 1000;
        }
        put_to_time_list(interval, &(memmove_statics[ds].interval_list));    
        memmove_statics[ds].last_called_time = t1;  
    }

    // memmove_times++;
    // // pthread_mutex_unlock(&memmove_lock);
    // DEBUG_MEMMOVE("memmove_times: %d\n", memmove_times);
    // if (memmove_times >= triger) {
    //     DEBUG_FILE("triger to write file\n", "");
    //     memmove_statics_to_file();                    
    //     memmove_times = 0;
    // }
    pthread_mutex_unlock(&memmove_lock);
    start_timer();
    return p;    
}

/*************************** memcpy ******************************************/

typedef memory_statics memcpy_statics_type;
static memcpy_statics_type memcpy_statics[GR_4M + 1];

static void *(*real_memcpy)(void *, const void *, size_t)=NULL;

static void memcpy_init(void) {
    DEBUG_MEMCPY("memcpy init\n", "");
    real_memcpy = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy");
    if (NULL == real_memcpy) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
}

void memcpy_statics_to_file() {
    statics_to_file(memcpy_latency_file, memcpy_latency_file_name.c_str(), 
        memcpy_interval_file, memcpy_interval_file_name.c_str(), memcpy_statics);
}

void *memcpy(void *str1, const void *str2, size_t size) {
    if (entry_local_func) {
        if (real_memcpy == NULL) {
            real_memcpy = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy");
        }
        return real_memcpy(str1, str2, size);
    } 
    init_flush_func();

    DEBUG_MEMCPY("memcpy %d\n", size); 
    #if ENABLE_TRACE
    trace(size);
    #endif

    pthread_mutex_lock(&memcpy_lock);    
    if (real_memcpy == NULL) {
        memcpy_init();
        for (int i = _1_64_; i < GR_4M; i++) {
            memcpy_statics[i].count = 0;
            memcpy_statics[i].latency_list.size = 0;
            memcpy_statics[i].latency_list.head = NULL;
            memcpy_statics[i].latency_list.tail = NULL;
            memcpy_statics[i].interval_list.size = 0;
            memcpy_statics[i].interval_list.head = NULL;
            memcpy_statics[i].interval_list.tail = NULL;
        }     
    }    

    // pthread_mutex_unlock(&memcpy_lock);

    

    // pthread_mutex_lock(&memcpy_lock);     

    DEBUG_MEMCPY("memcpy(%d)\n", size);

    enum data_size ds = check_data_size(size);
    DEBUG_MEMCPY("data_size(%d)\n", ds);
    memcpy_statics[ds].count = memcpy_statics[ds].count + 1;
    // pthread_mutex_unlock(&memcpy_lock);

    DEBUG_MEMCPY("memcpy %s count(%d)\n", data_size_str[ds], memcpy_statics[ds].count);
    
    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);
    void* p = real_memcpy(str1, str2, size);
    // stop timer
    gettimeofday(&t2, NULL);

    elapsedTime = (t2.tv_sec - t1.tv_sec);
    elapsedTime += (t2.tv_usec - t1.tv_usec);      
    DEBUG_MEMCPY("elapsedTime: %f us.\n", elapsedTime);

    // pthread_mutex_lock(&memcpy_lock);
    put_to_time_list(elapsedTime, &(memcpy_statics[ds].latency_list));

    if (memcpy_statics[ds].count == 1) { // it is called at the first time, no interval is record
        memcpy_statics[ds].last_called_time = t2;
    } else {// it is not called at the first time, caculate the interval from last called time
        uint64_t interval = (t1.tv_sec - memcpy_statics[ds].last_called_time.tv_sec);
        interval += (t1.tv_usec -  memcpy_statics[ds].last_called_time.tv_usec);
        if (interval > 60 * 1000 * 1000) {
            interval = 60 * 1000 * 1000;
        }
        put_to_time_list(interval, &(memcpy_statics[ds].interval_list));    
        memcpy_statics[ds].last_called_time = t1;  
    }

    pthread_mutex_unlock(&memcpy_lock);
    DEBUG_MEMCPY("finished memcpy %d\n", size);

    start_timer();
    
    return p;    
}

/******************************** timer **********************************************************/
// void* time_to_write_file(void *param) { 
//     DEBUG_TIMER("timer_is_activated %d \n", timer_is_activated);   
//     while (timer_is_activated) {
//         #if ENABLE_MALLOC
//         pthread_mutex_lock(&malloc_lock);
//         malloc_statics_to_file();
//         pthread_mutex_unlock(&malloc_lock);
//         #endif

//         pthread_mutex_lock(&memset_lock);
//         memset_statics_to_file();
//         pthread_mutex_unlock(&memset_lock);

//         pthread_mutex_lock(&memmove_lock);
//         memmove_statics_to_file();
//         pthread_mutex_unlock(&memmove_lock);

//         pthread_mutex_lock(&memcpy_lock);
//         memcpy_statics_to_file();
//         pthread_mutex_unlock(&memcpy_lock);
        
//         #if ENABLE_TRACE        
//         {
//             //const std::lock_guard<std::mutex> lock(trace_mutex);
//             int lock_times = 0;
//             //const std::unique_lock<std::mutex> lock(trace_mutex, std::try_to_lock);
//             while (!trace_mutex.try_lock_for(std::chrono::milliseconds(200))) {
//                 if (lock_times >= 5) {
//                     break;
//                 }
//                 lock_times++;
//             }          
//             entry_local_func += 1;
//             function_statics_to_file();
//             entry_local_func -= 1;
//             trace_mutex.unlock();
//         }
//         #endif

//         usleep(triger * 1000);
//     }    
// }

int to_file_count = 0;
void* time_to_write_file(void *param) {
    to_file_count++;
    DEBUG_TIMER("to_file_count is %d\n", to_file_count);
    if (to_file_count >= triger) {
        DEBUG_TIMER("write to file\n", "");
        #ifdef ENABLE_MALLOC
        pthread_mutex_lock(&malloc_lock);
        malloc_statics_to_file();
        pthread_mutex_unlock(&malloc_lock);
        #endif

        pthread_mutex_lock(&memset_lock);
        memset_statics_to_file();
        pthread_mutex_unlock(&memset_lock);

        pthread_mutex_lock(&memmove_lock);
        memmove_statics_to_file();
        pthread_mutex_unlock(&memmove_lock);

        pthread_mutex_lock(&memcpy_lock);
        memcpy_statics_to_file();
        pthread_mutex_unlock(&memcpy_lock);
        
        {
            //const std::lock_guard<std::mutex> lock(trace_mutex);
            int lock_times = 0;
            //const std::unique_lock<std::mutex> lock(trace_mutex, std::try_to_lock);
            while (!trace_mutex.try_lock_for(std::chrono::milliseconds(200))) {
                if (lock_times >= 5) {
                    break;
                }
                lock_times++;
            }          
            entry_local_func += 1;
            function_statics_to_file();
            entry_local_func -= 1;
            trace_mutex.unlock();
        }
        to_file_count = 0;        
    }     
}

int start_timer() {
    // pthread_mutex_lock(&timer_lock);
    // if (timer_is_activated) {
    //     pthread_mutex_unlock(&timer_lock); 
    //     return 0;
    // }

    // timer_is_activated = 1;
    // //pthread_t *tid = (pthread_t*)real_malloc(sizeof(pthread_t) );
    // pthread_t tid;    
    // pthread_create(&tid, NULL, time_to_write_file, NULL );    
    // DEBUG_TIMER("start time with timer_is_activated %d with tid %d\n", timer_is_activated, tid);
    // pthread_mutex_unlock(&timer_lock);  

    pthread_mutex_lock(&timer_lock);     
    time_to_write_file(NULL);
    pthread_mutex_unlock(&timer_lock);     
}