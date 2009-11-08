

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


int pthread_mutex_init(pthread_mutex_t * mutex , pthread_mutexattr_t * attr )
{
	LPCTSTR name = NULL;
	char bInitialOwner = 0;	// 1: mutex is owned by the thread calling this functions
	*mutex = CreateMutex(attr, bInitialOwner, name);
	return (*mutex != NULL);
}



int pthread_mutex_destroy(pthread_mutex_t * mutex )
{
	int r = CloseHandle( *mutex );
	return r;	//todo: check if return values match posix
}



int pthread_mutex_lock(pthread_mutex_t * mutex )
{
	DWORD dwWaitResult = WaitForSingleObject( *mutex, INFINITE);
	return dwWaitResult;	//todo: check if return values match posix
}



int pthread_mutex_unlock(pthread_mutex_t * mutex )
{
	int r = ReleaseMutex( *mutex );	// Returns !=0 on succes.
	char succes = (r!=0);
	r = succes ? 0 : -1;			// Convert to posix error: 0 means ok
	return r;
}


int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	InitializeSecurityDescriptor(attr, SECURITY_DESCRIPTOR_REVISION);
	return 0;
}


int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int kind)
{
	return 0;
}


//--------------- Since win32 mutexes are always recursive, provide an own implementation -------
// It uses InterlockedExchange() for thread security,
// SetEvent()/WaitForSingleObject() is used for inter-thread communication
#include <assert.h>

typedef struct {
	mutex_mode_e mode;	//thread policies, recursive or not.
	LONG	     lock;	//1: is locked, 0: not locked
	HANDLE	     owner;	//current thread that owns it
	HANDLE	     event; 	//event to wait for if it is locked
	int		    count;	//for debug: number of locks() by current thread on this mutex.
} mutex_t;



typedef struct {
	mutex_mode_e mode;
} mutexattr_t;


int mutexattr_settype(mutexattr_t *attr, mutex_mode_e kind)
{
	attr->mode = kind;
	return 0;
}



int mutex_init(mutex_t *mutex, mutexattr_t * attr )
{
	mutex->mode = attr->mode;
	mutex->lock = 0;
	mutex->owner = NULL;
	
	mutex->event = CreateEvent( 
        NULL,   // default security attributes
        FALSE,   // auto-reset event
        FALSE,  // initial state is nonsignaled
		NULL 	// name
		);
		
	if (mutex->event==0)
		return -1;
		
	mutex->count = 0;
		
	return 0;	//succes
}



int mutex_destroy(mutex_t * mutex )
{
	assert( mutex->owner == NULL);
	assert( mutex->count == 0);
	CloseHandle(mutex->event);
	return 0;
}



int mutex_lock(mutex_t * mutex )
{
	HANDLE owner = GetCurrentThread();
	//LONG lock;
	
	if( mutex->owner == owner && mutex->mode == PTHREAD_MUTEX_RECURSIVE )
	{
		mutex->count++;
	} 
	else 
	{
		// Since many threads can mutex_lock() simultaneously,
		// only the one that manages to change it from 0 to 1
		// has ownership.
		while( InterlockedExchange( &mutex->lock, 1) != 0)
		{
			// Wait for other thread to release it:
			DWORD r = WaitForSingleObject(mutex->event, INFINITE);
		}
	}
	
	// Now we own the thread, update status:
	mutex->count++;			//for debug
	mutex->owner = owner;
	
	return 0;
}



int mutex_unlock(mutex_t * mutex )
{
	HANDLE owner = GetCurrentThread();
	LONG lock;
	
	//Error checking:
	assert( owner == mutex->owner );	// Don't unlock anything we don't have:
	assert( mutex->count > 0);			// Don't unlock unlocked things
	assert( mutex->mode == PTHREAD_MUTEX_RECURSIVE || mutex->count < 2);	// only recursive can have more than 1 lock
	
	// Update state, just before releasing:
	mutex->count--;
	
	// For recursive mutexes, only unlock at the last unlock() call
	if( mutex->count == 0)
	{
		mutex->owner = NULL;
		lock = InterlockedExchange( &mutex->lock, 0);
	}
	
	//Fire off an event, to wake the waiting mutex_locks().
	SetEvent( mutex->event);
	return 0;
}


