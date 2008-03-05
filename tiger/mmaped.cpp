#include "mmaped.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <sys/mman.h>

int mmaped::open( const char* inputname ) {
	int err;
	
	fd = ::open( inputname, O_RDONLY );
	if ( fd <= 0 ) {
		perror(inputname);
		exit(1);
	}
	err = fstat( fd, &sb );
	if ( err == -1 ) {
		perror("fstat");
		exit(1);
	}
	{
		int pagesize = getpagesize();
		mmapsize = (sb.st_size + pagesize - 1) & (~(pagesize - 1));
#if __linux__ || __LINUX__ || linux || LINUX
#define MAP_FLAGS MAP_SHARED
#else
#define MAP_FLAGS (MAP_SHARED|MAP_FILE)
#endif
		//printf("mmap( NULL, mmapsize = 0x%x, PROT_READ, MAP_FILE, fd=%d, 0 );\n", mmapsize, fd );
		data = mmap( NULL, mmapsize, PROT_READ|PROT_EXEC, MAP_FLAGS, fd, 0 );
	}
	if ( (signed long)data == -1 ) {
		fprintf(stderr, "mmap returned data=%p\n", data );
		perror("mmap");
		exit(1);
	}
	return 0;
}

int mmaped::close() {
	if ( data != NULL && fd >= 0 ) {
		munmap( data, mmapsize );
		data = NULL;
		sb.st_size = 0;
		mmapsize = 0;
		int err = ::close( fd );
		fd = -1;
		return err;
	} else {
		return 0;
	}
}
