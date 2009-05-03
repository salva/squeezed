
/**
 * @file 
 * minimal posix-like wrapper for threading functions
 *
 *
 * \ingroup win32
 *@{
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_


//#define NOMINMAX
#include "windows.h"


#define restrict 
typedef HANDLE pthread_t;
typedef void pthread_attr_t;


/** Create a thread
*/
int pthread_create(pthread_t *restrict thread, //< output: the thread
	 const pthread_attr_t *restrict attr,	   //< {currently not used}
	 void *(*start_routine)(void *),		   //< pointer to function this thread executes
	 void *restrict arg					       //< pointer to arguments for start_routine()
	 )
{
	DWORD threadId;
	*thread = CreateThread( 
        NULL,                   // default security attributes
        0,                      // use default stack size  
        (LPTHREAD_START_ROUTINE)start_routine,			// thread function name
        arg,					// argument to thread function 
        0,                      // use default creation flags 
        &threadId);				// returns the thread identifier 

	return threadId;
}



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
pthread_exit(void *value_ptr)
{

}




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
int pthread_join(pthread_t thread, void **value_ptr)
{
	WaitForSingleObject(thread, INFINITE);
	return 0;
}

/*@}
 *end of doxygen group
 */



#endif