#include "uf1.h"

#include <stdio.h>

#if 0
char usage[] = "usage: uf1data infile outfile\n"
"\tinfile should be .uf1 census data\n"
"\toutfile will be the binary file the data is written to.\n".
#endif

int main(int argc, char** argv) {
	fprintf(stderr, "%s -> %s\n", argv[1], argv[2]);
	Uf1Data::processText(argv[1], argv[2]);
	return 0;
}
