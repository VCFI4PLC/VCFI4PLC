#ifndef _SHADOWSTACK_H_
#define _SHADOWSTACK_H_
#ifdef __cplusplus
extern "C" {


//#define errExit(msg) do { perror(msg); exit(errno);} while(0)


#define IOC_MAGIC  'c'

#define IOC_INIT    _IO(IOC_MAGIC, 0)
#define CMD(x) _IO(IOC_MAGIC,x)
#define SHADOWSTACK_PAGE

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


#ifdef SHADOWSTACK_TEEPLC
void my_pthread_detach(pthread_t thread);
int my_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
void my_pthread_join(pthread_t thread, void **retval);
void init_whole_shadow_stack();
void shadow_stack_save(unsigned long lr_value);
unsigned long shadow_stack_restore();
//unsigned long shadow_stack_restore_test(unsigned long lr_value);
////page part
extern pthread_key_t mytlskey;
extern int count;
extern int incount;
void my_pthread_exit(void *retval);
static void *getpage(void *ptr, size_t page_size);
void init_shadow_stack(pthread_t thread);
static void* thread_init(void *ptr);
void page_shadow_stack_save(unsigned long lr_value);
unsigned long page_shadow_stack_restore();
////
#endif


#ifdef __cplusplus
}
#endif

#endif
