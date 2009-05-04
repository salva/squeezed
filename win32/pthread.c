

//#define NOMINMAX
#include <windows.h>

#include "pthread.h"


/*----------------- Threads --------------------------------------------------*/


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



int pthread_join(pthread_t thread, void **value_ptr)
{
	int r = WaitForSingleObject(thread, INFINITE);
	return r;	//todo: check if return values match posix
}


/*----------------- Mutexes --------------------------------------------------*/


int pthread_mutex_init (pthread_mutex_t * mutex , pthread_mutexattr_t * attr )
{
	LPCTSTR name = NULL;
	char bInitialOwner = 1;	//mutex is owned by the thread calling this functions
	*mutex = CreateMutex(attr, bInitialOwner, name);
	return (mutex != NULL);
}



int pthread_mutex_destroy (pthread_mutex_t * mutex )
{
	int r = CloseHandle(mutex);
	return r;	//todo: check if return values match posix
}



int pthread_mutex_lock (pthread_mutex_t * mutex )
{
	DWORD dwWaitResult = WaitForSingleObject(mutex, INFINITE);
	return dwWaitResult;	//todo: check if return values match posix
}



int pthread_mutex_unlock (pthread_mutex_t * mutex )
{
	int r = ReleaseMutex( mutex );
	return r;	//todo: check if return values match posix
}