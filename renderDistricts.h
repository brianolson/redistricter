#ifndef RENDER_DISTRICTS_H
#define RENDER_DISTRICTS_H

//#include "config.h"
//#if WITH_PNG
void myDoPNG( const char* outname, unsigned char** rows, int height, int width ) __attribute__ (( weak ));
#pragma weak myDoPNG
void recolorDists( POPTYPE* adjacency, int adjlen, int numd, POPTYPE* renumber = NULL ) __attribute__ (( weak ));
#pragma weak recolorDists
//void renderDistricts( int numZips, double* xy, double* popRadii, POPTYPE* winner, char* outname, int height = 1000, int width = 1000 );
void printColoring( FILE* fout );
void readColoring( FILE* fin );
//#endif /* WITH_PNG */

#endif /* RENDER_DISTRICTS_H */
