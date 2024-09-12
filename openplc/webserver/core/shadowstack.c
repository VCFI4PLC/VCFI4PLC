#ifdef __cplusplus
extern "C" {

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


#include "shadowstack.h"


#if defined(SHADOWSTACK_TEEPLC)  
typedef int (*pthread_create_f)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
typedef struct
{
	void *(*start_routine)(void *);
	void *arg;
}FunctionCall;
typedef int (*pthread_detach_f)(pthread_t thread);
typedef int (*pthread_join_f)(pthread_t thread, void **retval);
typedef void (*pthread_exit_f)(void *retval);


static pthread_create_f real_pthread_create = NULL;
static pthread_detach_f real_pthread_detach = NULL;
static pthread_join_f real_pthread_join = NULL;
static pthread_exit_f real_pthread_exit = NULL;

static pthread_mutex_t fake_pthread_mutex;
static pthread_mutex_t ss_mutex;
////page part
pthread_key_t mytlskey;
static pthread_mutex_t ss_page_mutex;
////
int count = 0;
int incount = 0;
static pthread_mutex_t log_mutex;

page_size = 1000000;

static int fd = open("/dev/TEEPLC_DEVICE", O_RDWR);

typedef struct
{
	unsigned long pthread_t_value;
	int tid;
	int flag;
}fake_pthread_t;


static fake_pthread_t fake_pthread_list[100]={0};

static void add_fake_pthread(unsigned long thread, int tid)
{
	for(int i=0; i<100; i++){
		if((fake_pthread_list[i].pthread_t_value==0) || (fake_pthread_list[i].flag==0)){
			fake_pthread_list[i].pthread_t_value = thread;
			fake_pthread_list[i].tid = tid;
			fake_pthread_list[i].flag = 1;
			break;
		}
	}
	
	return;
}

static int get_fake_pthread(unsigned long thread)
{
	for(int i=0; i<100; i++){
		if(fake_pthread_list[i].pthread_t_value == thread){
			return fake_pthread_list[i].tid;
		}
	}
	//printf("Cannot find tid.\n");
	return -1;
}

static void free_fake_pthread(int tid)
{
	for(int i=0; i<100; i++){
		if(fake_pthread_list[i].tid == tid){
			fake_pthread_list[i].flag = 0;
			return;
		}
	}
	//printf("Cannot free tid.\n");
	return;
}

static void init_fake_pthread_list()
{
	memset(fake_pthread_list, 0, sizeof(fake_pthread_list));
}

void my_pthread_exit(void *retval)
{
	if(real_pthread_exit == NULL) real_pthread_exit = (pthread_exit_f)dlsym(RTLD_NEXT, "pthread_exit");
	//page part
	void *shadow_base = pthread_getspecific(mytlskey);
	int status = munmap(shadow_base, page_size);
	if(status == -1) printf("exit munmap error.");
	//
	real_pthread_exit(retval);
}

void my_pthread_detach(pthread_t thread)
{
	int subtid = get_fake_pthread((unsigned long)thread);

	//printf("detach getsubtid is %d.\n",subtid);
	unsigned int cc = CMD(0x4);
	//printf("end ioctl.\n");

	int *sub = &subtid;
	pthread_mutex_lock(&ss_mutex);
	int rc = ioctl(fd, cc, sub);
	pthread_mutex_unlock(&ss_mutex);
	//printf("detach ioc rc = %d.\n", rc);
	
	//page part
	void *shadow_base = pthread_getspecific(mytlskey);
	int status = munmap(shadow_base, page_size);
	if(status == -1) printf("detach munmap error.");
	//
	
	if(real_pthread_detach == NULL) real_pthread_detach = (pthread_detach_f)dlsym(RTLD_NEXT, "pthread_detach");
	real_pthread_detach(thread);
	
	
	pthread_mutex_lock(&fake_pthread_mutex);
	free_fake_pthread(subtid);	
	pthread_mutex_unlock(&fake_pthread_mutex);
}

void my_pthread_join(pthread_t thread, void **retval)
{
	if(real_pthread_join == NULL) real_pthread_join = (pthread_join_f)dlsym(RTLD_NEXT, "pthread_join");
	unsigned int cc = CMD(0x4);
	int subtid = get_fake_pthread((unsigned long)thread);
	int *sub = &subtid;
	//printf("join getsubtid is %d.\n",subtid);
	//printf("end ioctl.\n");
	pthread_mutex_lock(&ss_mutex);
	int rc = ioctl(fd, cc, sub);
	pthread_mutex_unlock(&ss_mutex);
	//printf("join ioc rc = %d.\n", rc);
	
	//page part
	void *shadow_base = pthread_getspecific(mytlskey);
	int status = munmap(shadow_base, page_size);
	if(status == -1) printf("join munmap error.");
	//
	real_pthread_join(thread, retval);
	
	pthread_mutex_lock(&fake_pthread_mutex);
	free_fake_pthread(subtid);	
	pthread_mutex_unlock(&fake_pthread_mutex);
}

static void *getpage(void *ptr, size_t page_size)
{
	return mmap(ptr, page_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

void init_shadow_stack(pthread_t thread)
{

	//printf("TID is %d.\n", gettid());
	unsigned int cc = CMD(0x1);
	//printf("start ioctl.\n");
	
	pthread_mutex_lock(&ss_mutex);
	int rc = ioctl(fd, cc);
	pthread_mutex_unlock(&ss_mutex);
	
	pthread_mutex_lock(&fake_pthread_mutex);
	add_fake_pthread((unsigned long)thread, (int)gettid());
	pthread_mutex_unlock(&fake_pthread_mutex);
	
	//page part
	void *shadowstack = getpage(NULL, page_size);
	pthread_setspecific(mytlskey, shadowstack);
	//
	//printf("start ioc rc = %d.\n", rc);

}

static void* thread_init(void *ptr)
{
	FunctionCall *call = (FunctionCall*)ptr;
	
	//printf("thread id is %d.\n", gettid());
	
	init_shadow_stack(pthread_self());
	
	//page part
	pthread_mutex_lock(&ss_page_mutex);
	void *shadowstack_base = pthread_getspecific(mytlskey);
	//asm volatile("mov x26, %0"::"r"((unsigned long)shadowstack_base):"x26");
	pthread_mutex_unlock(&ss_page_mutex);
	//	
	
	void *(*start_routine)(void*) = call->start_routine;
	void *arg = call->arg;
	free(call);
	void *ret = start_routine(arg);
	
	//page part
	//void *shadow_base = pthread_getspecific(mytlskey);
	//int status = munmap(shadow_base, 524288);
	//

	
	return ret;
}


int my_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	if(real_pthread_create == NULL){
		real_pthread_create = (pthread_create_f)dlsym(RTLD_NEXT, "pthread_create");
		printf("dlsym succeed! %s\n", dlerror());
	}

	FunctionCall *call = (FunctionCall*)malloc(sizeof(FunctionCall));
	call->start_routine = start_routine;
	call->arg = arg;
	//printf("Functioncall succeed.\n");
	
	int err = real_pthread_create(thread, attr, thread_init, (void*)call);
	
	if(err != 0) { free(call); printf("free succeed.\n");}
	printf("my_pthread_create end.\n");
	return err;		
}


void init_whole_shadow_stack(){
	
	if (pthread_mutex_init(&fake_pthread_mutex, NULL) != 0)
    {
        //printf("Mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&ss_mutex, NULL) != 0)
    {
        //printf("Mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&log_mutex, NULL) != 0)
    {
        //printf("Mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&ss_page_mutex, NULL) != 0)
    {
        //printf("Mutex init failed\n");
        exit(1);
    }
	init_fake_pthread_list();
	unsigned int cc = CMD(0x5);
	//printf("start ioctl.\n");
	int rc = ioctl(fd, cc);
	//printf("start ioc rc = %d.\n", rc);
	init_shadow_stack(pthread_self());
}

void shadow_stack_save(unsigned long lr_value){
	
	//printf("now start shadow stack saving.\n");
	unsigned int cc = CMD(0x2);
	pthread_mutex_lock(&log_mutex);
	incount+=1;
	pthread_mutex_unlock(&log_mutex);
	unsigned long *lr_pointer = (unsigned long*)malloc(0x100);
	*lr_pointer = lr_value;
	//printf("%ld\n", lr_value);
	//PRINT_CALLER_FUNC
	//printf("save ioctl.\n");
	//printf("function name is %s.\n", func_name);
	int rc = ioctl(fd, cc, lr_pointer);
	//printf("save ioc rc = %d.\n", rc);
	
	free(lr_pointer);
}

unsigned long shadow_stack_restore(){
	//printf("then restore shadow stack.\n");
	
	unsigned int cc = CMD(0x3);
	pthread_mutex_lock(&log_mutex);
	count+=1;
	pthread_mutex_unlock(&log_mutex);
	unsigned long *lr_pointer = (unsigned long*)malloc(0x100);
	
	//printf("restore ioctl.\n");
	int rc = ioctl(fd, cc, lr_pointer);
	//printf("restore ioc rc = %d.\n", rc);
	
	unsigned long lr = (unsigned long)(*lr_pointer);
	free(lr_pointer);
	//printf("restore step. lr is %ld.\n", lr);
	return lr;
	
}


////page part
void page_shadow_stack_save(unsigned long lr_value){
	unsigned int tid = gettid();
	//printf("now start shadow stack saving.\n");
	
	pthread_mutex_lock(&ss_page_mutex);
	void *shadow_base = pthread_getspecific(mytlskey);
	*shadow_base = lr_value;
	//asm volatile("str %0, [x26]"::"r"(lr_value):"x26");
	//asm volatile("add x26, x26, #8":::"x26");
	pthread_mutex_unlock(&ss_page_mutex);
	
	pthread_mutex_lock(&log_mutex);
	const char *logFilePath = "/home/pi/OpenPLC_v3/webserver/core/mylogstore.txt";
	FILE *logFile = fopen(logFilePath, "a");
	//printf("fopen success.\n");
	fprintf(logFile,"%d: %ld\n",tid,lr_value);
	fclose(logFile);
	pthread_mutex_unlock(&log_mutex);
}
unsigned long page_shadow_stack_restore(){
	unsigned int tid = gettid();
	//printf("then restore shadow stack.\n");
	pthread_mutex_lock(&ss_page_mutex);
	unsigned long *lr_pointer=(unsigned long*)malloc(sizeof(unsigned long));
	unsigned long lr_value;
	//asm volatile("sub x26, x26, #8":::"x26");
	//asm volatile("ldr %0, [x26]":"=r"(lr_value)::"x26");
	pthread_mutex_unlock(&ss_page_mutex);
	
	pthread_mutex_lock(&log_mutex);
	const char *logFilePath = "/home/pi/OpenPLC_v3/webserver/core/mylogrestore.txt";
	FILE *logFile = fopen(logFilePath, "a");
	//printf("fopen success.\n");
	fprintf(logFile,"%d: %ld\n",tid,lr_value);
	fclose(logFile);
	pthread_mutex_unlock(&log_mutex);
	
	return lr_value;
}
////
#endif

#endif




#ifdef __cplusplus
}
#endif
