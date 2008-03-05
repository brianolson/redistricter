#include "Bitmap.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

Bitmap::Bitmap( int initSize ) {
	int numl = (initSize/sizeof(u_int32_t)) + 1;
	bits = new u_int32_t[numl];
	bytes = sizeof(u_int32_t) * numl;
}
Bitmap::~Bitmap() {
	if ( bits ) {
		delete [] bits;
	}
}
void Bitmap::zero() {
	memset( bits, 0, bytes );
}
void Bitmap::flood() {
	memset( bits, 0xff, bytes );
}
int Bitmap::writeBin(int fd) {
	int err;
	err = write( fd, &bytes, sizeof(size_t) );
	if ( err < sizeof(size_t) ) {
		perror("Bitmap::writeBin size");
		::close(fd);
		return -1;
	}
	err = write( fd, bits, bytes );
	if ( err < bytes ) {
		perror("Bitmap::writeBin data");
		::close(fd);
		return -1;
	}
	return 0;
}
int Bitmap::readBin(int fd) {
	int err;
	size_t nsize;
	u_int32_t* nbits;
	err = read( fd, &nsize, sizeof(size_t) );
	if ( err != sizeof(size_t) ) {
		perror("Bitmap::readBin size");
		::close(fd);
		return -1;
	}
	nbits = new u_int32_t[nsize/sizeof(u_int32_t)];
	err = read( fd, nbits, nsize );
	if ( err != nsize ) {
		perror("Bitmap::readBin data");
		::close(fd);
		return -1;
	}
	if ( bits != NULL ) {
		delete [] bits;
		bits = nbits;
		bytes = nsize;
	}
	return 0;
}
