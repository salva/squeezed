
/**
 * @file 
 * Minimal posix-like wrapper for threading functions
 *
 * It is not complete, but provides basic functionality
 *
 * TODO:
 * Since win32 does not have a non-recursive mutex, maybe copy
 * the pthreads solution of using InterLockedExchange and events ??
 *
 * \ingroup win32
 *@{
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

//#define NOMINMAX
#include <windows.h>
#define restrict 

#ifdef __cplusplus
extern "C" {
#endif

/*----------------- Threads --------------------------------------------------*/

typedef HANDLE pthread_t;
typedef void pthread_attr_t;

typedef enum { PTHREAD_MUTEX_DEFAULT, 
				PTHREAD_MUTEX_NORMAL, 
				PTHREAD_MUTEX_RECURSIVE, 
				PTHREAD_MUTEX_ERRORCHECK, 
				PTHREAD_MUTEX_OWNERTERM_NP 
} mutex_mode_e;


/** Create a thread
*/
int pthread_create(pthread_t *restrict thread, //< output: the thread
	 const pthread_attr_t *restrict attr,	   //< {currently not used}
	 void *(*start_routine)(void *),		   //< pointer to function this thread executes
	 void *restrict arg					       //< pointer to arguments for start_routine()
	 );



/** is currently not implemented
The pthread_exit() function terminates the calling thread and makes the
value value_ptr available to any successful join with the terminating
thread.  Any cancellation cleanup handlers that have been pushed and are
not yet popped are popped in the reverse order that they were pushed and
then executed.  After all cancellation handlers have been executed, if
the thread has any thread-specific data, appropriate destructor functions
are called in an unspecified order.  Thread termination does not release
any application visible process resources, including, but not limited to,
mutexes and file descriptors, nor does it perform any process level
cleanup actions, including, but not limited to, calling atexit() routines
that may exist.
*/
void
pthread_exit(void *value_ptr);




/** The pthread_join() function suspends execution of the calling thread
until the target thread terminates, unless the target thread has already
terminated.

On return from a successful pthread_join() call with a non-NULL value_ptr
argument, the value passed to pthread_exit() by the terminating thread is
stored in the location referenced by value_ptr.  When a pthread_join()
returns successfully, the target thread has been terminated.  The results
of multiple simultaneous calls to pthread_join(), specifying the same
target thread, are undefined.  If the thread calling pthread_join() is
cancelled, the target thread is not detached.

RETURN VALUES

If successful,  the pthread_join() function will return zero.  Otherwise,
an error number will be returned to indicate the error.
*/
int pthread_join(pthread_t thread, void **value_ptr);


/*----------------- Mutexes --------------------------------------------------*/


typedef HANDLE pthread_mutex_t;
typedef SECURITY_ATTRIBUTES pthread_mutexattr_t;


/** The pthread_mutex_init function initializes the given mutex with the given attributes. 
	If attr is null, then the default attributes are used. 

	Returns
		The pthread_mutex_init function returns zero if the call is successful, otherwise it sets errno to EINVAL and returns -1.
	Implementation Notes
	The argument attr must be null. The default attributes are always used. 
*/
int  pthread_mutex_init (pthread_mutex_t * mutex , pthread_mutexattr_t * attr );



int  pthread_mutex_destroy (pthread_mutex_t * mutex );




/**The pthread_mutex_lock function locks the given mutex. 
If the mutex is already locked, then the calling thread blocks until the thread that 
currently holds the mutex unlocks it.

The pthread_mutex_lock function returns zero if the call is successful,
otherwise it sets errno to EINVAL and returns -1. 
*/
int  pthread_mutex_lock (pthread_mutex_t * mutex );



/**The pthread_mutex_unlock function unlocks the given mutex.

The pthread_mutex_unlock function returns zero if the call is successful, 
otherwise it sets errno to EINVAL and returns -1. 
*/
int  pthread_mutex_unlock (pthread_mutex_t * mutex );


int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr); 
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind);



#ifdef __cplusplus
}
#endif 


#endif	//pthread_h


/*@}
 *end of doxygen group
 */
