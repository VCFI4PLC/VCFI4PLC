//-----------------------------------------------------------------------------
// Copyright 2018 Thiago Alves
// This file is part of the OpenPLC Software Stack.
//
// OpenPLC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OpenPLC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with OpenPLC.  If not, see <http://www.gnu.org/licenses/>.
//------
//
// This is the main file for the OpenPLC. It contains the initialization
// procedures for the hardware, network and the main loop
// Thiago Alves, Jun 2018
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <json/json.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <fstream>
#include <list>
#include "shadowstack.h"
#include "iec_types.h"
#include "ladder.h"
#include "vulner_func.h"

#define PRINT_CALLER_FUNC printf("Caller's func : %s\n", __func__);

#ifdef _ethercat_src
#include "ethercat_src.h"
#endif
#define NSEC_PER_SEC 1000000000 //1s 1000ms
#define OPLC_CYCLE          50000000
#if defined(SHADOWSTACK_PAGE) || defined(SHADOWSTACK_TEEPLC) 
extern pthread_key_t mytlskey;
extern int count;
extern int incount;
#endif
extern int opterr;
//extern int common_ticktime__;
IEC_BOOL __DEBUG;

IEC_LINT cycle_counter = 0;

unsigned long __tick = 0;
pthread_mutex_t bufferLock; //mutex for the internal buffers
pthread_mutex_t logLock; //mutex for the internal log
uint8_t run_openplc = 1; //Variable to control OpenPLC Runtime execution
uint32_t cycles = 10000;
unsigned char log_buffer[1000000]; //A very large buffer to store all logs
int log_index = 0;
int log_counter = 0;



struct timeRecord{
    long cycle_t;
    long delay_t;
};
std::map<int, timeRecord> timeList; 

static inline uint64_t timespec_to_uint64(struct timespec *t)
{
    return static_cast<uint64_t>(t->tv_sec) * NSEC_PER_SEC + static_cast<uint64_t>(t->tv_nsec);
}
static inline uint64_t uint64_to_timespec(uint64_t input, struct timespec *t)
{
    uint64_t nsecs = input % NSEC_PER_SEC;
    uint64_t secs = (input-nsecs)/ NSEC_PER_SEC;
    t->tv_sec = static_cast<uint64_t>(secs);
    t->tv_nsec = static_cast<uint64_t>(nsecs);
}
//-----------------------------------------------------------------------------
// Helper function - Makes the running thread sleep for the ammount of time
// in milliseconds
//-----------------------------------------------------------------------------
void sleep_until(struct timespec *ts, long long delay)
{
    ts->tv_sec  += delay / NSEC_PER_SEC;
    ts->tv_nsec += delay % NSEC_PER_SEC;
    if(ts->tv_nsec >= NSEC_PER_SEC)
    {
        ts->tv_nsec -= NSEC_PER_SEC;
        ts->tv_sec++;
    }
    
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ts,  NULL);
    //printf("ts->tv_sec is %ld\n", ts->tv_sec);
    //printf("ts->tv_nsec is %ld\n", ts->tv_nsec);
}

//-----------------------------------------------------------------------------
// Helper function - Makes the running thread sleep for the ammount of time
// in milliseconds
//-----------------------------------------------------------------------------
void sleepms(int milliseconds)
{
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}

/**
 * @fn timespec_diff(struct timespec *, struct timespec *, struct timespec *)
 * @brief Compute the diff of two timespecs, that is a - b = result.
 * @param a the minuend
 * @param b the subtrahend
 * @param result a - b
 */
static inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result) {
    result->tv_sec  = a->tv_sec  - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

//-----------------------------------------------------------------------------
// Helper function - Logs messages and print them on the console
//-----------------------------------------------------------------------------
void log(unsigned char *logmsg)
{
    pthread_mutex_lock(&logLock); //lock mutex
    printf("%s", logmsg);
    for (int i = 0; logmsg[i] != '\0'; i++)
    {
        log_buffer[log_index] = logmsg[i];
        log_index++;
        log_buffer[log_index] = '\0';
    }
    
    log_counter++;
    if (log_counter >= 1000)
    {
        /*Store current log on a file*/
        log_counter = 0;
        log_index = 0;
    }
    pthread_mutex_unlock(&logLock); //unlock mutex
}



//-----------------------------------------------------------------------------
// Interactive Server Thread. Creates the server to listen to commands on
// localhost
//-----------------------------------------------------------------------------
void *interactiveServerThread(void *arg)
{
    startInteractiveServer(43628);
}

//-----------------------------------------------------------------------------
// Verify if pin is present in one of the ignored vectors
//-----------------------------------------------------------------------------
bool pinNotPresent(int *ignored_vector, int vector_size, int pinNumber)
{
    for (int i = 0; i < vector_size; i++)
    {
        if (ignored_vector[i] == pinNumber)
            return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
// Disable all outputs
//-----------------------------------------------------------------------------
void disableOutputs()
{
    //Disable digital outputs
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (bool_output[i][j] != NULL) *bool_output[i][j] = 0;
        }
    }
    
    //Disable byte outputs
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (byte_output[i] != NULL) *byte_output[i] = 0;
    }
    
    //Disable analog outputs
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (int_output[i] != NULL) *int_output[i] = 0;
    }
}

//-----------------------------------------------------------------------------
// Special Functions
//-----------------------------------------------------------------------------
void handleSpecialFunctions()
{
    //current time [%ML1024]
    struct tm *current_time;
    time_t rawtime;
    
    time(&rawtime);
    // store the UTC clock in [%ML1027]
    if (special_functions[3] != NULL) *special_functions[3] = rawtime;

    current_time = localtime(&rawtime);
    
    rawtime = rawtime - timezone;
    if (current_time->tm_isdst > 0) rawtime = rawtime + 3600;
        
    if (special_functions[0] != NULL) *special_functions[0] = rawtime;
    
    //number of cycles [%ML1025]
    cycle_counter++;
    if (special_functions[1] != NULL) *special_functions[1] = cycle_counter;
    
    //comm error counter [%ML1026]
    /* Implemented in modbus_master.cpp */

    //insert other special functions below
}

//-----------------------------------------------------------------------------
// Using special_functions to store REAL-TIME variables
//-----------------------------------------------------------------------------
void RecordCycletimeLatency(long cycle_time, long sleep_latency)
{
    if (special_functions[4] != NULL) *special_functions[4] = cycle_time;
    if (special_functions[5] != NULL) *special_functions[5] = sleep_latency;
}

// pointers to IO *array[const][const] from cpp to c and back again don't work as expected, so instead callbacks
u_int8_t *bool_input_call_back(int a, int b){ return bool_input[a][b]; }
u_int8_t *bool_output_call_back(int a, int b){ return bool_output[a][b]; }
u_int8_t *byte_input_call_back(int a){ return byte_input[a]; }
u_int8_t *byte_output_call_back(int a){ return byte_output[a]; }
u_int16_t *int_input_call_back(int a){ return int_input[a]; }
u_int16_t *int_output_call_back(int a){ return int_output[a]; }
u_int32_t *dint_input_call_back(int a){ return dint_input[a]; }
u_int32_t *dint_output_call_back(int a){ return dint_output[a]; }
u_int64_t *lint_input_call_back(int a){ return lint_input[a]; }
u_int64_t *lint_output_call_back(int a){ return lint_output[a]; }
void logger_callback(unsigned char *msg){ log(msg);}

int main(int argc,char **argv)
{
    
#ifdef SHADOWSTACK_PAGE
    pthread_key_create(&mytlskey, NULL);
    //set shadowstack for main function.
    init_whole_shadow_stack();
    //void *main_shadowstack_base = pthread_getspecific(mytlskey);
	//asm volatile("mov x26, %0"::"r"((unsigned long)main_shadowstack_base):"x26");
#endif

#ifdef SHADOWSTACK_TEEPLC
	pthread_key_create(&mytlskey, NULL);
    init_whole_shadow_stack();
	//void *main_shadowstack_base = pthread_getspecific(mytlskey);
	//asm volatile("mov x26, %0"::"r"((unsigned long)main_shadowstack_base):"x26");    
#endif 

    // Define the max/min/avg/total cycle and latency variables used in REAL-TIME computation(in nanoseconds)
    long cycle_avg, cycle_max, cycle_min, cycle_total;
    long latency_avg, latency_max, latency_min, latency_total;
    cycle_max = 0;
    cycle_min = LONG_MAX;
    cycle_total = 0;
    latency_max = 0;
    latency_min = LONG_MAX;
    latency_total = 0;

    unsigned char log_msg[100];
    sprintf((char *)log_msg, "OpenPLC Runtime starting...\n");
    log(log_msg);
    

    //======================================================
    //                 PLC INITIALIZATION
    //======================================================
    tzset();
    time(&start_time);
    pthread_t interactive_thread;
#if defined(SHADOWSTACK_PAGE) || defined(SHADOWSTACK_TEEPLC) 
    my_pthread_create(&interactive_thread, NULL, interactiveServerThread, NULL);
#else
    pthread_create(&interactive_thread, NULL, interactiveServerThread, NULL);
#endif  
    config_init__();
    glueVars();

    //======================================================
    //               MUTEX INITIALIZATION
    //======================================================
    if (pthread_mutex_init(&bufferLock, NULL) != 0)
    {
        printf("Mutex init failed\n");
        exit(1);
    }

    //======================================================
    //              HARDWARE INITIALIZATION
    //======================================================
#ifdef _ethercat_src
    type_logger_callback logger = logger_callback; 
    ethercat_configure("../utils/ethercat_src/build/ethercat.cfg", logger);
#endif
    initializeHardware();
    initializeMB();
    initCustomLayer();
    updateBuffersIn();
    updateCustomIn();
    updateBuffersOut();
    updateCustomOut();

    //======================================================
    //          PERSISTENT STORAGE INITIALIZATION
    //======================================================
    glueVars();
    mapUnusedIO();
    readPersistentStorage();
    //pthread_t persistentThread;
    //pthread_create(&persistentThread, NULL, persistentStorage, NULL);
    


#ifdef __linux__
    //======================================================
    //              REAL-TIME INITIALIZATION
    //======================================================
    // Set our thread to real time priority
    struct sched_param sp;
    sp.sched_priority = 99;
    printf("Setting main thread priority to RT\n");
    if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp))
    {
        printf("WARNING: Failed to set main thread to real-time priority\n");
    }

    // Lock memory to ensure no swapping is done.
    printf("Locking main thread memory\n");
    if(mlockall(MCL_FUTURE|MCL_CURRENT))
    {
        printf("WARNING: Failed to lock memory\n");
    }
#endif
	
	// Define the start, end, cycle time and latency time variables
	struct timespec cycle_start, cycle_end, cycle_time;
	struct timespec timer_start, timer_end, sleep_latency;

	//gets the starting point for the clock
	printf("Getting current time\n");
	printf("common_ticktime__ is %d\n", common_ticktime__);//cycle time 2000 000 000
	clock_gettime(CLOCK_MONOTONIC, &timer_start);
	//printf("time_start: %ld\n", timespec_to_uint64(&timer_start));
	//======================================================
	//                    MAIN LOOP
	//======================================================
	//printf("cycles is %d\n", cycles);
	while(cycles)
	{
		cycles -= 1;
		// Get the start time for the running cycle
		clock_gettime(CLOCK_MONOTONIC, &cycle_start);
		//printf("cycle_start: %ld\n", timespec_to_uint64(&cycle_start));
		//make sure the buffer pointers are correct and
		//attached to the user variables
		glueVars();
        
#ifdef _ethercat_src
        boolvar_call_back bool_input_callback = bool_input_call_back;
        boolvar_call_back bool_output_callback = bool_output_call_back;
        int8var_call_back byte_input_callback = byte_input_call_back;
        int8var_call_back byte_output_callback = byte_output_call_back;
        int16var_call_back int_input_callback = int_input_call_back;
        int16var_call_back int_output_callback = int_output_call_back;
        int32var_call_back dint_input_callback = dint_input_call_back;
        int32var_call_back dint_output_callback = dint_output_call_back;
        int64var_call_back lint_input_callback = lint_input_call_back;
        int64var_call_back lint_output_callback = lint_output_call_back;
#endif
        
		updateBuffersIn(); //read input image

		pthread_mutex_lock(&bufferLock); //lock mutex


#ifdef _ethercat_src
        if(ethercat_callcyclic(BUFFER_SIZE, 
                bool_input_callback, 
                bool_output_callback, 
                byte_input_callback, 
                byte_output_callback, 
                int_input_callback, 
                int_output_callback, 
                dint_input_call_back, 
                dint_output_call_back, 
                lint_input_call_back, 
                lint_output_call_back)){
            printf("EtherCAT cyclic failed\n");
            break;
        }
#endif
		updateCustomIn();
        updateBuffersIn_MB(); //update input image table with data from slave devices
        handleSpecialFunctions();
		config_run__(__tick++); // execute plc program logic
		updateCustomOut();
        updateBuffersOut_MB(); //update slave devices with data from the output image table
		pthread_mutex_unlock(&bufferLock); //unlock mutex

		updateBuffersOut(); //write output image
        
		updateTime();
		
		// Get the end time for the running cycle
		clock_gettime(CLOCK_MONOTONIC, &cycle_end);
		//printf("cycle_end: %ld\n", timespec_to_uint64(&cycle_end));
		// Compute the time usage in one cycle and do max/min/total comparison/recording
		timespec_diff(&cycle_end, &cycle_start, &cycle_time);
//#if defined(SHADOWSTACK_PAGE) || defined(SHADOWSTACK_TEEPLC) 
//		if (__tick % 200 == 0)
//		    printf("cycle_time: %ld, incount is %d, count is %d\n", timespec_to_uint64(&cycle_time), incount, count);
//#endif
		
		//char input[100];
		//snprintf(input, sizeof(input), "cycle_time: %ld\n", timespec_to_uint64(&cycle_time));
		//vulner_func(input);
		long cycle_time_print = timespec_to_uint64(&cycle_time);
		long remaining_time = common_ticktime__ - cycle_time_print;
		//printf("cycle_time: %ld\n", cycle_time_print);
		
		//printf("remaining_time: %ld\n", remaining_time);
		//if (remaining_time >= 20000000){	}
			
		
		
		if (cycle_time.tv_nsec > cycle_max)
			cycle_max = cycle_time.tv_nsec;
		if (cycle_time.tv_nsec < cycle_min)
			cycle_min = cycle_time.tv_nsec;
		cycle_total = cycle_total + cycle_time.tv_nsec;
		
				
		
		//long next_cycle_time = timespec_to_uint64(&timer_start) + common_ticktime__;
		//printf("next cycle: %ld\n", next_cycle_time);
		sleep_until(&timer_start, common_ticktime__);
		
		// Get the sleep end point which is also the start time/point of the next cycle
		clock_gettime(CLOCK_MONOTONIC, &timer_end);
		//printf("time end: %ld\n", timespec_to_uint64(&timer_end));
		// Compute the time latency of the next cycle(caused by sleep) and do max/min/total comparison/recording
		timespec_diff(&timer_end, &timer_start, &sleep_latency);
		//printf("sleep_latency: %ld\n",timespec_to_uint64(&sleep_latency));
		if (sleep_latency.tv_nsec > latency_max)
			latency_max = sleep_latency.tv_nsec;
		if (sleep_latency.tv_nsec < latency_min)
			latency_min = sleep_latency.tv_nsec;
		latency_total = latency_total + sleep_latency.tv_nsec;

		// Store the cycle_time/sleep_latency in microsecond, so it can be displayed in the webpage
		//RecordCycletimeLatency((long)cycle_time.tv_nsec / 1000, (long)sleep_latency.tv_nsec / 1000);
		timeList[__tick] = {(long)cycle_time.tv_nsec / 1000, (long)sleep_latency.tv_nsec / 1000};
	}
	
	// Compute/print the max/min/avg cycle time and latency
	cycle_avg = (long)cycle_total / __tick;
	latency_avg = (long)latency_total / __tick;
	printf("###Summary: The maximum/minimum/average cycle time in microsecond is %ld/%ld/%ld\n",
	cycle_max / 1000, cycle_min / 1000, cycle_avg / 1000);
	printf("###Summary: The maximum/minimum/average latency in microsecond is %ld/%ld/%ld\n",
	latency_max / 1000,   latency_min / 1000, latency_avg / 1000);
	
	
	
	//printf("now cycles is %d\n", cycles);
	
	Json::Value time_root;
        Json::StreamWriterBuilder time_writerBuilder;
        Json::Value time_item;
        for(const auto &item : timeList){
            //std::cout<<std::endl;
            time_item["id"] = item.first;
	    time_item["ticktime"] = (long)common_ticktime__/1000;
            time_item["cycle_t"] = item.second.cycle_t;
            time_item["delay_t"] = item.second.delay_t;
            time_root.append(time_item);
        }
        std::ofstream outputFile("../json/time_record.json", std::ios::out | std::ios::trunc);
        if(outputFile.is_open()){
            std::string json_str = Json::writeString(time_writerBuilder,time_root);
            outputFile << json_str;
            outputFile.close();
        }else{
            printf("error write Json file.\n");
        }	

    //======================================================
	//             SHUTTING DOWN OPENPLC RUNTIME
	//======================================================
#if defined(SHADOWSTACK_PAGE) || defined(SHADOWSTACK_TEEPLC)
    my_pthread_detach(interactive_thread);
    my_pthread_join(interactive_thread, NULL);
#else
    pthread_detach(interactive_thread);
    pthread_join(interactive_thread, NULL);
#endif


    //pthread_exit(interactive_thread);
    printf("pthread_join\n");
#ifdef _ethercat_src
    ethercat_terminate_src();
#endif
    printf("Disabling outputs\n");
    disableOutputs();
    updateCustomOut();
    updateBuffersOut();
	finalizeHardware();
    printf("Shutting down OpenPLC Runtime...\n");
    
#if defined(SHADOWSTACK_PAGE) || defined(SHADOWSTACK_TEEPLC)
    my_pthread_detach(pthread_self());
#endif

#if defined(SHADOWSTACK_PAGE) || defined(SHADOWSTACK_TEEPLC)
    pthread_key_delete(mytlskey);
#endif
    exit(0);
}
