# mstatics 

This tool is used to statistics the C memory function usage. Currently the supported C memory function is memcpy, memset, memmove and malloc. This use the C/C++ LD_PRELOAD to override the C memory function, so that the application code needn't change any code to use the tool.

The statistics result is generated to two excel file. The project provide a tool to generate some pictures to summary the memory function usage in the report file.

![image info](./Capture.PNG)

Also the tool is able to report the memory function trace stack:

![image info](./Capture_trace.PNG)

## How to Build

* cd path/to/mstatic/
* make

## Test 
* cd /path/to/mstatic/test
* LD_PRELOAD=/path/to/mstatics/lib/libmstatics.so ./mtest

## How to use
*  LD_PRELOAD=/path/to/mstatics/lib/libmstatics.so *application*

## How to generate the report
* cd /path/to/mstatic/src
* ./processfile.py

## Eviroment variable
* MSTATICS_OUT_DIR: this variable is used to specify the directory for the report to generate.
* TIMER_TO_LOG: this variable is used to specify how often the tool to sample the memory function usage. The unit is ms. For example, "export TIMER_TO_LOG=2000" means the tool will append a statistics line in the report file every 2 seconds.

