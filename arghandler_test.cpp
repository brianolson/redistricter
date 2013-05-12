#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "arghandler.h"

const char* targv[] =
{
    "progname",
    "-c=1",
    "-d", "2",
    "-f=4",
    "--g=5",
    "--e", "3"
};

int test(int argc, char** argv) {
    int argi = 1;
    char* c;
    char* d;
    char* e;
    char* f;
    char* g;
    
    while (argi < argc) {
	StringArgWithCopy("c", &c);
	StringArgWithCopy("d", &d);
	StringArgWithCopy("e", &e);
	StringArgWithCopy("f", &f);
	StringArgWithCopy("g", &g);
	assert(0); // this should never be reached, macros should always `continue`
    }

    assert(0 == strcmp(c, "1"));
    assert(0 == strcmp(d, "2"));
    assert(0 == strcmp(e, "3"));
    assert(0 == strcmp(f, "4"));
    assert(0 == strcmp(g, "5"));

    return 0;
}

int main(int argc, char** argv) {

    test(sizeof(targv)/sizeof(char*), (char**)targv);

    return 0;
}
