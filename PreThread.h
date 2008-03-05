#ifndef PRETHREAD_H
#define PRETHREAD_H

#include <pthread.h>

class PreThread {
protected:
	pthread_t thread;
	void* (*foo)(void*);
	void* arg;
	void* retval;
	pthread_mutex_t mut;
	pthread_cond_t start;
	pthread_cond_t stop;
	
public:
	PreThread();
	~PreThread();

	/* is there currently a function here? */
	bool busy();
	
	/* do it! */
	int run( void* (*foo)(void*), void* arg );
	
	/*  */
	void* join();
	
protected:
	void* func();
	
	friend void* ::PreThread_func( void* arg );
};

#endif /* PRETHREAD_H */
