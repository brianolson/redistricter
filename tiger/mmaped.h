#ifndef mmaped_H
#define mmaped_H

#include <sys/stat.h>
#include <sys/types.h>

class mmaped {
public:
	int fd;
	void* data;
	struct stat sb;
	size_t mmapsize;

        /** return 0 on success, -1 on error
        (in some theory, but as implemented it calls exit() on error) */
	int open( const char* name );
	int close();
};

#endif
