#include "PreThread.h"


void* PreThread_func( void* arg ) {
	PreThread* p = (PreThread*)arg;
	return p->func();
}

void* PreThread::func() {
	pthread_mutex_lock( &mut );
	while ( 1 ) {
		pthread_cond_wait( &start, &mut );
		if ( foo == PreThread_func ) {
			// this is the signal to quit
			break;
		}
		if ( foo != NULL ) {
			pthread_mutex_unlock( &mut );
			retval = foo( arg );
			pthread_mutex_lock( &mut );
			foo = NULL;
			pthread_cond_broadcast( &stop );
		}
	}
	pthread_mutex_unlock( &mut );
	return NULL;
}

PreThread::PreThread() {
	pthread_mutex_init( &mut, NULL );
	pthread_cond_init( &start, NULL );
	pthread_cond_init( &stop, NULL );
	foo = NULL;
	arg = NULL;
	retval = NULL;
	pthread_create( &thread, NULL, PreThread_func, this );
}

PreThread::~PreThread() {
	run( PreThread_func, NULL );
	pthread_join( thread, NULL );
	pthread_mutex_destroy( &mut );
	pthread_cond_destroy( &start );
	pthread_cond_destroy( &stop );
}

bool PreThread::busy() {
	bool toret;
	pthread_mutex_lock( &mut );
	toret = (foo != NULL);
	pthread_mutex_unlock( &mut );
	return toret;
}
int PreThread::run( void* (*fooIn)(void*), void* argIn ) {
	pthread_mutex_lock( &mut );
	while ( foo != NULL ) {
		pthread_cond_wait( &stop, &mut );
	}
	foo = fooIn;
	arg = argIn;
	pthread_cond_signal( &start );
	pthread_mutex_unlock( &mut );
	return 0;
}

void* PreThread::join() {
	void* toret;
	pthread_mutex_lock( &mut );
	while ( foo != NULL ) {
		pthread_cond_wait( &stop, &mut );
	}
	toret = retval;
	pthread_mutex_unlock( &mut );
	return toret;
}
