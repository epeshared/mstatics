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
#include <sstream>


//libdwfl
// #include <cxxabi.h> // __cxa_demangle
// #include <elfutils/libdwfl.h> // Dwfl*
// #include <execinfo.h> // backtrace
// #include <map>


/********************** lock *****************************/

std::timed_mutex trace_mutex; 
std::mutex function_file_lock;
std::mutex init_lock;

void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, -1, 0);
}

int initialise_shared() {
    entry_local_func++;
    // place our shared data in shared memory
    DEBUG_INIT("initialise shared data\n", "");
    

    // char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);        
    // if (tmp_out_dir != NULL) {
    //     out_dir = std::string(tmp_out_dir);
    //     DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    // } else {
    //     out_dir = "./";
    //     DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    // }

    // std::string shared_data_lock_name = out_dir + "/shared_lock";

    // // FILE* shared_lock_fd = fopen(shared_data_lock_name.c_str(),"w");
    // // if (shared_lock_fd == NULL) {
    // //     log_error("Error opening lock file:%s!\n" ,shared_data_lock_name.c_str());
    // //     exit(1);
    // // }
    // // fclose(shared_lock_fd);
    // int zero = 0;
    // int fd = open(shared_data_lock_name.c_str(), O_CREAT|O_RDWR,S_IRWXU);
    // write(fd,&zero,sizeof(int));
    // if (fd == -1) {
    //     log_error("Error opening lock file:%s!\n" ,shared_data_lock_name.c_str());
    //     exit(1);
    // }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    memory_usage_data = (memory_usage_t *)mmap(NULL, sizeof(memory_usage_t), prot, flags, -1, 0);
    assert(memory_usage_data);
    // close(fd);

    memory_usage_data->index = 0;

    // initialise mutex so it works properly in shared memory
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&memory_usage_data->mutex, &attr);
    entry_local_func--;
    return 0;
}

int initialise_trace_data() {
    entry_local_func++;
    // place our shared data in shared memory
    DEBUG_INIT("initialise trace data\n", "");

    // char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);        
    // if (tmp_out_dir != NULL) {
    //     out_dir = std::string(tmp_out_dir);
    //     DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    // } else {
    //     out_dir = "./";
    //     DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    // }

    // std::string trace_lock_name = out_dir + "/trace_lock";

    // int zero = 0;
    // int fd = open(trace_lock_name.c_str(), O_CREAT|O_RDWR,S_IRWXU);
    // write(fd,&zero,sizeof(int));
    // if (fd == -1) {
    //     log_error("Error opening lock file:%s!\n" ,trace_lock_name.c_str());
    //     exit(1);
    // }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    trace_record = (trace_data_t *)mmap(NULL, sizeof(trace_data_t), prot, flags, -1, 0);
    assert(trace_record);
    // close(fd);
    

    trace_record->index = 0;

    // initialise mutex so it works properly in shared memory
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&trace_record->mutex, &attr);
    entry_local_func--;
    return 0;
}

int initialise_init_data() {
    entry_local_func++;
    // place our shared data in shared memory
    DEBUG_INIT("initialise init data\n", "");
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    init_data = (init_data_t *)mmap(NULL, sizeof(init_data_t), prot, flags, -1, 0);
    assert(init_data);

    init_data->init = false;

    // initialise mutex so it works properly in shared memory
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&init_data->mutex, &attr);
    DEBUG_INIT("finish initialise init data\n", "");
    entry_local_func--;
    return 0;
}

// int initialise_timer() {
//     entry_local_func++;
//     // place our shared data in shared memory
//     DEBUG_INIT("initialise timer\n", "");
//     int prot = PROT_READ | PROT_WRITE;
//     int flags = MAP_SHARED | MAP_ANONYMOUS;
//     timer_s = (timer_status_t *)mmap(NULL, sizeof(timer_status_t), prot, flags, -1, 0);
//     assert(timer_s);

//     timer_s->start = false;

//     // initialise mutex so it works properly in shared memory
//     pthread_mutexattr_t attr;
//     pthread_mutexattr_init(&attr);
//     pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
//     pthread_mutex_init(&timer_s->mutex, &attr);
//     DEBUG_INIT("finish initialise init data\n", "");
//     entry_local_func--;
//     return 0;    
// }

bool is_initialized() {
    entry_local_func++;
    // if (!init_data) {
    //     initialise_init_data();
    // }
    pthread_mutex_lock(&init_data->mutex);
    bool ret = false;
    if (init_data->init) {
        ret = true;
    } else {
        ret = false;
    }
    pthread_mutex_unlock(&init_data->mutex);
    entry_local_func--;
    return ret;        
}

int initialize() {
    entry_local_func++;
    pthread_mutex_lock(&init_data->mutex);
    if (init_data->init) {
        pthread_mutex_unlock(&init_data->mutex);
        return 0;
    }

    std::string pid = std::to_string(getpid());
    DEBUG_INIT("pid %s starting init\n", pid.c_str());

    real_malloc = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
    if (NULL == real_malloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    } 

    real_memcpy = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy"); 
    if (NULL == real_memcpy) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }     

    real_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
    if (NULL == real_memset) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }

    real_memmove = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memmove");
    if (NULL == real_memmove) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }           

    memory_usage_file_name = "memory_usage.csv";
    function_trace_file_name = "function_trace.csv"; 


    char* tmp_out_dir = getenv (MSTATICS_OUT_DIR);        
    if (tmp_out_dir != NULL) {
        out_dir = std::string(tmp_out_dir);
        DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    } else {
        out_dir = "./";
        DEBUG_INIT("out_dir: %s\n", out_dir.c_str());
    }
    
    //std::cout << "DEBUG: init function called process " << ::getpid() << std::endl;                

    // gettimeofday(&last_flush_time, NULL);

    char* COUNT_TO_LOG_STR = getenv(TIMER_TO_LOG);
    if (COUNT_TO_LOG_STR != NULL) {
        triger = atoi(COUNT_TO_LOG_STR);            
    }
    DEBUG_INIT("TIMER_TO_LOG is change to %d\n", triger);

    char memory_usage_header[] = "time,type,size,count,latency";        
    memory_usage_file_name = out_dir + memory_usage_file_name;
    DEBUG_INIT("memory_usage_file_name: %s\n", memory_usage_file_name.c_str());     
    memory_usage_file = fopen(memory_usage_file_name.c_str(),"w");
    if (memory_usage_file== NULL) {
        DEBUG_INIT("Error opening malloc_latency_file file!\n" ,"");
        exit(1);
    }        
    fprintf(memory_usage_file, "%s\n", memory_usage_header);
    fclose(memory_usage_file);

    char tracing_header[] = "time,function,size";        
    function_trace_file_name = out_dir + function_trace_file_name;
    DEBUG_INIT("memory_usage_file_name: %s\n", function_trace_file_name.c_str());     
    function_trace_file = fopen(function_trace_file_name.c_str(),"w");
    if (function_trace_file== NULL) {
        DEBUG_INIT("Error opening function_trace_file file!\n" ,"");
        exit(1);
    }        
    fprintf(function_trace_file, "%s\n", tracing_header);
    fclose(function_trace_file);
    DEBUG_INIT("pid %s finished init\n", pid.c_str());
    
    //start_timer();
    init_data->init = true;    

    start_timer();

    pthread_mutex_unlock(&init_data->mutex);

    entry_local_func--;
    return 0;
}


/********************** stack trace **********************/

void trace_stack(mem_func_type mem_func, size_t tracing_size) {
    entry_local_func++;
    pthread_mutex_lock(&trace_record->mutex);
    DEBUG_TRACE("---------------------------------------------------------\n", "");

    if (!(mem_func == memory_copy && tracing_size >= 4*1024*1024)) {
        return;
    }

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
            if (ss_str.compare("trace_stack(unsigned long)") == 0) {
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
    DEBUG_TRACE("::::1\n", "");
    int size_range = check_data_size(tracing_size);

    char fmt[64];
    struct timeval  tv;
    struct tm *tm;
    DEBUG_TRACE("::::2\n", "");
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    DEBUG_TRACE("::::3\n", "");
    
    //trace_record_t record = trace_record->record[trace_record->index];
    if(tm != NULL) {
        strftime(fmt, sizeof fmt, "%Y-%m-%d-%H:%M:%S.%%06u", tm);
        snprintf(trace_record->record[trace_record->index].time_buffer, sizeof(trace_record->record[trace_record->index].time_buffer), fmt, tv.tv_usec);
        //free(tm);
    }
    DEBUG_TRACE("::::4 trace_record->index %d, call stack %s\n", trace_record->index, call_statck.c_str());
   

    int n = call_statck.length();
    DEBUG_TRACE("::::5 n %d, max length\n", n);
    int index = trace_record->index;
    DEBUG_TRACE("::::6 index %d\n", index);
    // trace_record->record[index].function_stack = (char *)create_shared_memory(n + 1);
    DEBUG_TRACE("trace_record->record[index] address : %p\n", trace_record->record[index]);
    DEBUG_TRACE("trace_record->record[index].function_stack address : %p\n", trace_record->record[index].function_stack);
    size_t length = call_statck.copy(trace_record->record[index].function_stack, n + 1, 0);
    trace_record->record[index].function_stack[length] = '\0';
    // trace_record->record[index].function_stack = call_statck;
    DEBUG_TRACE("::::8\n", "");
    //DEBUG_TRACE("::::7 index %d callstack %s\n", index, trace_record->record[index].function_stack.c_str());
    DEBUG_TRACE("record.function_stack %s\n",trace_record->record[index].function_stack);
    trace_record->record[index].size = tracing_size;
    trace_record->index++;

    if (trace_record->index >= MAX_RECORD_NUM) {
        DEBUG_TRACE("start to flush trace with index %d\n", trace_record->index);
        DEBUG_FILE("write to file:%s\n", function_trace_file_name.c_str());

        function_trace_file = fopen(function_trace_file_name.c_str(),"a");
        
        
        for (int i = 0;i < trace_record->index; i++) {
            std::ostringstream os;
            trace_record_t record = trace_record->record[i];            
            DEBUG_TRACE("i: %d\n", i);
            if (trace_record->record[i].function_stack == NULL) {
                DEBUG_TRACE("the i %d function statck is null, skipp\n", i);
                continue;
            }
            DEBUG_TRACE("time: %s\n", trace_record->record[i].time_buffer);
            DEBUG_TRACE("trace_record->record[index] address : %p\n", trace_record->record[i]);
            DEBUG_TRACE("trace_record->record[index].function_stack address : %p\n", trace_record->record[i].function_stack);            
            try
            {
               DEBUG_TRACE("function_stack: %s\n", trace_record->record[i].function_stack);
            }
            catch (std::exception& e)
            {
                std::cerr << "Exception caught : " << e.what() << std::endl;
            }            
            
            DEBUG_TRACE("size: %d\n", trace_record->record[i].size);        
            os << trace_record->record[i].time_buffer << "," << trace_record->record[i].function_stack << "," << trace_record->record[i].size;
            //std::string str(os.str());
            DEBUG_TRACE("--->add line %s\n",os.str().c_str());
            fprintf(function_trace_file, "%s\n", os.str().c_str());
        }


        // for (int ii = 0;ii < trace_record->index; ii++) {
        //     //free(trace_record->record[ii].function_stack);
        //     real_memset(trace_record->record[ii].function_stack, 0 , sizeof trace_record->record[ii].function_stack);
        //     real_memset(trace_record->record[ii].time_buffer,0, sizeof trace_record->record[ii].time_buffer);
        //     trace_record->record[ii].size = 0;            
        // } 
        trace_record->index = 0;     
        fclose(function_trace_file);
    } 
    // else {
    //     for (int i = 0;i < trace_record->index; i++) {
    //         std::ostringstream os;
    //         //trace_record_t record = trace_record->record[i];            
    //         DEBUG_TRACE("i: %d\n", i);
    //         DEBUG_TRACE("time: %s\n", trace_record->record[i].time_buffer);
    //         DEBUG_TRACE("function_stack: %s\n", trace_record->record[i].function_stack.c_str());
    //         DEBUG_TRACE("size: %d\n", trace_record->record[i].size);        
    //         os << trace_record->record[i].time_buffer << "," << trace_record->record[i].function_stack << "," << trace_record->record[i].size;
    //         std::string str(os.str());
    //         DEBUG_TRACE("--->insert line %s\n",os.str().c_str());
    //     }        
    // }
    DEBUG_TRACE("---------------------------------------------------------\n", "");
    pthread_mutex_unlock(&trace_record->mutex);
    entry_local_func--;
}

// int write_to_trace_file_and_clean() {
//     entry_local_func++;

//     DEBUG_FILE("write to file:%s\n", function_trace_file_name.c_str());

//     function_trace_file = fopen(function_trace_file_name.c_str(),"a");
    
//     for (int i = 0;i < trace_record->index; i++) {
//         std::ostringstream os;
//         //trace_record_t record = trace_record->record[i];
//         DEBUG_TRACE("time: %s\n", trace_record->record[i].time_buffer);
//         DEBUG_TRACE("function_stack: %s\n", trace_record->record[i].function_stack);
//         DEBUG_TRACE("size: %d\n", trace_record->record[i].size);        
//         os << trace_record->record[i].time_buffer << "," << trace_record->record[i].function_stack << "," << trace_record->record[i].size;
//         //std::string str(os.str());
//         DEBUG_TRACE("--->add line %s\n",os.str().c_str());
//         fprintf(function_trace_file, "%s\n", os.str().c_str());      
//     }

//     for (int ii = 0;ii < trace_record->index; ii++) {
//         //free(trace_record->record[ii].function_stack);
//         trace_record->record[ii].function_stack.clear();
//         real_memset(trace_record->record[ii].time_buffer,0, sizeof trace_record->record[ii].time_buffer);
//         trace_record->record[ii].size = 0;            
//     } 
//     trace_record->index = 0;     
//     fclose(function_trace_file);
//     entry_local_func--;
//     return 0;
// }

/********************** log ******************************/
static const char *level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};


static const char *data_size_str[] = {
    "1_64", "65_128", "129_256", "257_512",
    "513_1K", "1K_2K", "2K_4K","4K_8K", "8K_16K",
    "16K_32K", "32K_64K", "128K_256K", "256K_512K",
    "512K_1M", "1M_2M", "2M_4M", ">4M"
};


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

// static char function_trace_header[] = "function,1-64,65-128,129-256,257-512,513-1K,1K-2K,2K-4K,4K-8K,8K-16K,16K-32K,32K-64K,128K-256K,256K-512K,512K-1M,1M-2M,2M-4M,>4M";

int write_to_memory_usage_file_and_clean() {
    entry_local_func++;
    DEBUG_FILE("write to file:%s\n", memory_usage_file_name.c_str());

    memory_usage_file = fopen(memory_usage_file_name.c_str(),"a");
    
    #if MERGE_OUTPUT
    memmory_usage_aggre_record_t memcpy_aggr_record;
    memmory_usage_aggre_record_t memmove_aggr_record;
    memmory_usage_aggre_record_t memset_aggr_record;

    for (int i = 0;i < memory_usage_data->index; i++) {
        memmory_usage_record_t record = memory_usage_data->record[i];
        std::string type(record.type);

        data_size ds = check_data_size(record.size);
        if (type.compare("memcpy") == 0) {
            memcpy_aggr_record.count[ds] += 1;
            memcpy_aggr_record.latency[ds] += record.latency;
            memcpy_aggr_record.size[ds] += record.size;
        } else if (type.compare("memset") == 0) {
            memset_aggr_record.count[ds] += 1;
            memset_aggr_record.latency[ds] += record.latency;
            memset_aggr_record.size[ds] += record.size;
        } else if (type.compare("memmove") == 0) {
            memmove_aggr_record.count[ds] += 1;
            memmove_aggr_record.latency[ds] += record.latency;
            memmove_aggr_record.size[ds] += record.size;
        } else {
            log_error("can not support type:%s\n", type.c_str());
            exit(-1);
        }
    }

    char* timebuf = memory_usage_data->record[(memory_usage_data->index) - 1].time_buffer; 

    for (int i = 0; i <= GR_4M ; i++ ) {
        if (memcpy_aggr_record.count[i] != 0) {
            std::ostringstream os;
            uint64_t count = memcpy_aggr_record.count[i];
            uint64_t latency = (memcpy_aggr_record.latency[i] / count);
            uint64_t size = (memcpy_aggr_record.size[i] / count);
            os << timebuf << "," << "memcpy" << "," << data_size_str[i] << "," << count << ","<< latency;
            std::string str(os.str());
            //DEBUG_TRACE("add line %s\n",str.c_str());
            fprintf(memory_usage_file, "%s\n", str.c_str());               
        }

        if (memmove_aggr_record.count[i] != 0) {
            std::ostringstream os;
            uint64_t count = memmove_aggr_record.count[i];
            uint64_t latency = (memmove_aggr_record.latency[i] / memmove_aggr_record.count[i]);
            uint64_t size = (memmove_aggr_record.size[i] / memmove_aggr_record.count[i]);
            os << timebuf << "," << "memmove" << "," << data_size_str[i] << "," << count << ","<< latency;
            std::string str(os.str());
            //DEBUG_TRACE("add line %s\n",str.c_str());
            fprintf(memory_usage_file, "%s\n", str.c_str());   
        } 

        if (memset_aggr_record.count[i] != 0) {
            std::ostringstream os;
            uint64_t count = memset_aggr_record.count[i];
            uint64_t latency = (memset_aggr_record.latency[i] / memset_aggr_record.count[i]);
            uint64_t size = (memset_aggr_record.size[i] / memset_aggr_record.count[i]);
            os << timebuf << "," << "memset" << "," << data_size_str[i] << ","<< count << ","<< latency;
            std::string str(os.str());
            //DEBUG_TRACE("add line %s\n",str.c_str());
            fprintf(memory_usage_file, "%s\n", str.c_str());               
        }                  
    }
    #else    
    for (int i = 0;i < memory_usage_data->index; i++) {
        std::ostringstream os;
        memmory_usage_record_t record = memory_usage_data->record[i];

        os << record.time_buffer << "," << record.type << "," << record.size << "," << record.latency;
        std::string str(os.str());
        //DEBUG_TRACE("add line %s\n",str.c_str());
        fprintf(memory_usage_file, "%s\n", str.c_str());             
    } 
    #endif
    fclose(memory_usage_file);
    entry_local_func--;

    return 0;
}



/*************************** memset ******************************************/

void *memset(void *str, int c, size_t size) {
  

    if (entry_local_func) {
        //DEBUG_MEMSET("Local memset(%d) \n", size); 
        if (real_memset == NULL) {
            real_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
            if (NULL == real_memset) {
                fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
            }
        }
        //DEBUG_MEMSET("finished Local memset(%d) \n", size); 
        return real_memset(str, c, size);
    }

    if (!is_initialized()) {
        if (real_memset == NULL) {
            real_memset = (void* (*)(void*, int, size_t))dlsym(RTLD_NEXT, "memset");
            if (NULL == real_memset) {
                fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
            }
        }
        //DEBUG_MEMSET("finished memset(%d) is not init\n", size); 
        return real_memset(str, c, size);
    }
    DEBUG_MEMSET("memset(%d)\n", size); 
          
    #if ENABLE_TRACE
    trace_stack(memory_set,size);
    #endif

    char fmt[64];
    struct tm *tm;  
    
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
    
    gettimeofday(&t1, NULL);
    tm = localtime(&t1.tv_sec);

    pthread_mutex_lock(&memory_usage_data->mutex);
    //memmory_usage_record_t record = memory_usage_data->record[memory_usage_data->index];
    memory_usage_data->record[memory_usage_data->index].latency = elapsedTime;
    memory_usage_data->record[memory_usage_data->index].size = size;

    if(tm != NULL) {
        strftime(fmt, sizeof fmt, "%Y-%m-%d-%H:%M:%S.%%06u", tm);
        snprintf(memory_usage_data->record[memory_usage_data->index].time_buffer, sizeof(memory_usage_data->record[memory_usage_data->index].time_buffer), fmt, t1.tv_usec);
    }
    
    char type[]="memset";
    real_memcpy(memory_usage_data->record[memory_usage_data->index].type, type, strlen(type));
    memory_usage_data->index++;
    DEBUG_MEMSET("index %d\n",memory_usage_data->index);

    if (memory_usage_data->index >= MAX_RECORD_NUM) {
        write_to_memory_usage_file_and_clean();
        for (int i = 0;i < memory_usage_data->index; i++) {
            //memmory_usage_record_t record = memory_usage_data->record[i];
            memory_usage_data->record[i].latency = 0.0f;
            memory_usage_data->record[i].size = 0;
            real_memset( memory_usage_data->record[i].time_buffer, 0, sizeof  memory_usage_data->record[i].time_buffer);
            real_memset( memory_usage_data->record[i].type, 0, sizeof  memory_usage_data->record[i].type);         
        }        
        memory_usage_data->index = 0;
    }

    pthread_mutex_unlock(&memory_usage_data->mutex);
    DEBUG_MEMSET("finished MEMSET\n","");
    return p;    
}

void *memmove(void *str1, const void *str2, size_t size) {
     
    if (entry_local_func) {
        //DEBUG_MEMMOVE("Local memmove(%d) \n", size); 
        if (real_memmove == NULL) {
           real_memmove = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memmove");
        }
        //DEBUG_MEMMOVE("finished Local memmove(%d) \n", size); 
        return real_memmove(str1, str2, size);
    }

    if (!is_initialized()) {
        if (real_memmove == NULL) {
           real_memmove = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memmove");
        }
       // DEBUG_MEMMOVE("finished memmove(%d) is not init\n", size); 
        return real_memmove(str1, str2, size);
    }

    DEBUG_MEMMOVE("memmove(%d)\n", size);  
        
    #if ENABLE_TRACE
    trace_stack(memory_move, size);
    #endif

    char fmt[64];
    struct tm *tm;  
    
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
    
    gettimeofday(&t1, NULL);
    tm = localtime(&t1.tv_sec);

    pthread_mutex_lock(&memory_usage_data->mutex);
    //memmory_usage_record_t record = memory_usage_data->record[memory_usage_data->index];
    memory_usage_data->record[memory_usage_data->index].latency = elapsedTime;
    memory_usage_data->record[memory_usage_data->index].size = size;

    if(tm != NULL) {
        strftime(fmt, sizeof fmt, "%Y-%m-%d-%H:%M:%S.%%06u", tm);
        snprintf(memory_usage_data->record[memory_usage_data->index].time_buffer, sizeof(memory_usage_data->record[memory_usage_data->index].time_buffer), fmt, t1.tv_usec);
    }
    
    char type[]="memmove";
    real_memcpy(memory_usage_data->record[memory_usage_data->index].type, type, strlen(type));

    memory_usage_data->index++;
    DEBUG_MEMMOVE("index %d\n",memory_usage_data->index);

    if (memory_usage_data->index >= MAX_RECORD_NUM) {
        write_to_memory_usage_file_and_clean();
        for (int i = 0;i < memory_usage_data->index; i++) {
            //memmory_usage_record_t record = memory_usage_data->record[i];
            memory_usage_data->record[i].latency = 0.0f;
            memory_usage_data->record[i].size = 0;
            real_memset( memory_usage_data->record[i].time_buffer, 0, sizeof  memory_usage_data->record[i].time_buffer);
            real_memset( memory_usage_data->record[i].type, 0, sizeof  memory_usage_data->record[i].type);         
        }        
        memory_usage_data->index = 0;
    }

    pthread_mutex_unlock(&memory_usage_data->mutex);
    DEBUG_MEMMOVE("finished MEMMOVE\n","");
    return p; 
}


void *memcpy(void *str1, const void *str2, size_t size) {   

    if (entry_local_func) {
        //DEBUG_MEMCPY("Local memcpy(%d) \n", size); 
        if (real_memcpy == NULL) {
            real_memcpy = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy");
        }
        //DEBUG_MEMCPY("finished Local memcpy(%d) \n", size); 
        return real_memcpy(str1, str2, size);
    }

    if (!is_initialized()) {
       // DEBUG_MEMCPY("memcpy(%d) is not init\n", size); 
        if (real_memcpy == NULL) {
            real_memcpy = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy");
        }
        // DEBUG_MEMCPY("finished memcpy(%d) is not init\n", size); 
        return real_memcpy(str1, str2, size);
    }

    DEBUG_MEMCPY("memcpy(%d) \n", size);  

    if (entry_local_func) {
        if (real_memcpy == NULL) {
            real_memcpy = (void* (*)(void*, const void*, size_t))dlsym(RTLD_NEXT, "memcpy");
        }
        return real_memcpy(str1, str2, size);
    } 
    
    #if ENABLE_TRACE
    trace_stack(memory_copy, size);
    #endif

    char fmt[64];
    struct tm *tm;  
    
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
    
    gettimeofday(&t1, NULL);
    tm = localtime(&t1.tv_sec);

    pthread_mutex_lock(&memory_usage_data->mutex);
    //memmory_usage_record_t record = memory_usage_data->record[memory_usage_data->index];
    memory_usage_data->record[memory_usage_data->index].latency = elapsedTime;
    memory_usage_data->record[memory_usage_data->index].size = size;

    if(tm != NULL) {
        strftime(fmt, sizeof fmt, "%Y-%m-%d-%H:%M:%S.%%06u", tm);
        snprintf(memory_usage_data->record[memory_usage_data->index].time_buffer, sizeof(memory_usage_data->record[memory_usage_data->index].time_buffer), fmt, t1.tv_usec);
    }
    
    char type[]="memcpy";
    real_memcpy(memory_usage_data->record[memory_usage_data->index].type, type, strlen(type));
    memory_usage_data->index++;
    DEBUG_MEMCPY("index %d\n",memory_usage_data->index);

    if (memory_usage_data->index >= MAX_RECORD_NUM) {
        write_to_memory_usage_file_and_clean();
        for (int i = 0;i < memory_usage_data->index; i++) {
            //memmory_usage_record_t record = memory_usage_data->record[i];
            memory_usage_data->record[i].latency = 0.0f;
            memory_usage_data->record[i].size = 0;
            real_memset( memory_usage_data->record[i].time_buffer, 0, sizeof  memory_usage_data->record[i].time_buffer);
            real_memset( memory_usage_data->record[i].type, 0, sizeof  memory_usage_data->record[i].type);         
        }
        memory_usage_data->index = 0;
    }

    pthread_mutex_unlock(&memory_usage_data->mutex);
    DEBUG_MEMCPY("finished MEMCPY\n","");
    return p; 
}

/******************************** timer **********************************************************/
void* time_to_write_file(void *param) { 
    DEBUG_TIMER("timer_is_activated %d \n", timer_is_activated);   
    while (timer_is_activated) {
        DEBUG_TIMER("timer start to write\n", ""); 
        pthread_mutex_lock(&memory_usage_data->mutex);
        DEBUG_TIMER("timer write memory usage\n", ""); 
        write_to_memory_usage_file_and_clean();
        for (int i = 0;i < memory_usage_data->index; i++) {
            //memmory_usage_record_t record = memory_usage_data->record[i];
            memory_usage_data->record[i].latency = 0.0f;
            memory_usage_data->record[i].size = 0;
            real_memset( memory_usage_data->record[i].time_buffer, 0, sizeof  memory_usage_data->record[i].time_buffer);
            real_memset( memory_usage_data->record[i].type, 0, sizeof  memory_usage_data->record[i].type);         
        }
        memory_usage_data->index = 0;
        pthread_mutex_unlock(&memory_usage_data->mutex);        

#if ENABLE_TRACE
        pthread_mutex_lock(&trace_record->mutex);
        DEBUG_TIMER("timer write trace\n", ""); 
        function_trace_file = fopen(function_trace_file_name.c_str(),"a");
        
        DEBUG_TIMER("timer write trace 2\n", ""); 
        for (int i = 0;i < trace_record->index; i++) {
            DEBUG_TIMER("timer write trace 3\n", ""); 
            DEBUG_TIMER("timer write trace 4\n", ""); 
            // trace_record_t record = trace_record->record[i];            
            DEBUG_TIMER("i: %d\n", i);
            if (trace_record->record[i].function_stack == NULL) {
                DEBUG_TIMER("the i %d function statck is null, skipp\n", i);
                continue;
            }
            DEBUG_TIMER("time: %s\n", trace_record->record[i].time_buffer);
            // DEBUG_TIMER("trace_record->record[index] address : %p\n", trace_record->record[i]);
            DEBUG_TIMER("trace_record->record[index].function_stack address : %p\n", trace_record->record[i].function_stack);            
            try {
               DEBUG_TIMER("function_stack: %s\n", trace_record->record[i].function_stack);
            }
            catch (std::exception& e)
            {
                std::cerr << "Exception caught : " << e.what() << std::endl;
            }            
            
            char *t_buff = trace_record->record[i].time_buffer;
            char *f_buff = trace_record->record[i].function_stack;
            uint64_t size = trace_record->record[i].size;
            DEBUG_TIMER("time: %s\n", t_buff);
            DEBUG_TIMER("trace_record->record[index].function_stack address : %s\n", f_buff);   
            DEBUG_TIMER("size: %d\n", size);        
            // os << t_buff << "," << f_buff << "," << size;
            DEBUG_TIMER("timer write trace 4.5\n", "");
            //std::string str(os.str());
            // DEBUG_TIMER("--->add line %s\n",os.str().c_str());
            fprintf(function_trace_file, "%s,%s,%d\n", t_buff, f_buff, size);
        }
        
        DEBUG_TIMER("timer write trace 5\n", "");

        // for (int ii = 0;ii < trace_record->index; ii++) {
        //     //free(trace_record->record[ii].function_stack);
        //     real_memset(trace_record->record[ii].function_stack, 0 , sizeof trace_record->record[ii].function_stack);
        //     real_memset(trace_record->record[ii].time_buffer,0, sizeof trace_record->record[ii].time_buffer);
        //     trace_record->record[ii].size = 0;            
        // } 
        trace_record->index = 0;     
        fclose(function_trace_file);
        DEBUG_TIMER("timer write trace 6\n", ""); 
        pthread_mutex_unlock(&trace_record->mutex);
        DEBUG_TIMER("timer finish writing trace\n", ""); 
#endif        
        usleep(triger * 1000);
    }    
}

int start_timer() {
    pthread_t tid;
    timer_is_activated = true;    
    pthread_create(&tid, NULL, time_to_write_file, NULL );    
    DEBUG_TIMER("start time with timer_is_activated %d with tid %d\n", timer_is_activated, tid); 
}