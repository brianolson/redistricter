#ifndef BITMAP_H
#define BITMAP_H

#include <sys/types.h>

class Bitmap {
public:
	Bitmap( int initSize );
	~Bitmap();
	size_t bytes;
	u_int32_t* bits;
	
	inline bool test( int bit ) {
		return (bits[bit/32] & (1 << (bit % 32))) != 0;
	}
	inline void set( int bit ) {
		bits[bit/32] |= (1 << (bit % 32));
	}
	inline void clear( int bit ) {
		bits[bit/32] &= ~(1 << (bit % 32));
	}
	void zero();
	void flood();
	int writeBin(int fd);
	int readBin(int fd);
};

#endif
