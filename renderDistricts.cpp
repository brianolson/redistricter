#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>

#include "config.h"
#include "renderDistricts.h"

const unsigned char stdColors[] = {
	0xff, 0x00, 0x00,	// 0 red
	0x00, 0xff, 0x00,	// 1 green
	0x00, 0x00, 0xff,	// 2 blue
	0xff, 0xff, 0x00,	// 3 yellow
	0xff, 0x00, 0xff,	// 4 magenta
	0x00, 0xff, 0xff,	// 5 cyan
	0x00, 0x00, 0x00,	// 6 black
	//0xff, 0xff, 0xff,	// 6 white
	0x99, 0x99, 0x99,	// 7 grey
	0xff, 0x80, 0x00,	// 8 orange
	0xff, 0x00, 0x80,	// 9 pink
	0xff, 0x80, 0x80,
	0x80, 0x80, 0x00,
	0x80, 0x00, 0x80,
	0x00, 0xff, 0x80,
	0x00, 0x80, 0xff,
	0x80, 0xff, 0x00,
	0x80, 0xff, 0x80,
	0x80, 0x00, 0xff,
	0x80, 0x80, 0xff,
};
const int numStdColors = sizeof(stdColors) / ( sizeof(unsigned char) * 3 );

const unsigned char* colors = stdColors;
int numColors = numStdColors;

#if WITH_PNG

#include <png.h>

png_voidp user_error_ptr = 0;
void user_error_fn( png_structp png_ptr, const char* str ) {
	fprintf( stderr, "error: %s", str );
}
void user_warning_fn( png_structp png_ptr, const char* str ) {
	fprintf( stderr, "warning: %s", str );
}

void myDoPNG( const char* outname, unsigned char** rows, int height, int width ) {
	FILE* fout;
	
	fout = fopen( outname, "wb");
	if ( fout == NULL ) {
		perror( outname );
		exit(1);
	}

	png_structp png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING,
		(png_voidp)user_error_ptr, user_error_fn, user_warning_fn );
	if (!png_ptr) {
		fclose(fout);
		exit( 2 );
	}
	
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct( &png_ptr, (png_infopp)NULL );
		fclose(fout);
		exit( 2 );
	}
	
	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fout);
		exit( 2 );
	}
	
	png_init_io(png_ptr, fout);
	
	/* set the zlib compression level */
	png_set_compression_level(png_ptr,
		Z_BEST_COMPRESSION);
	
	png_set_IHDR(png_ptr, info_ptr, width, height,
		/*bit_depth*/8,
		/*color_type*/PNG_COLOR_TYPE_RGBA,
		/*interlace_type*/PNG_INTERLACE_NONE,
		/*compression_type*/PNG_COMPRESSION_TYPE_DEFAULT,
		/*filter_method*/PNG_FILTER_TYPE_DEFAULT);
	
	png_write_info(png_ptr, info_ptr);
	png_write_image( png_ptr, rows );
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose( fout );
}

static int recolorFixup( POPTYPE* adjacency, int adjlen, POPTYPE* ci, int numd, int index, POPTYPE* fixupOrderArray ) {
#define fixupOrder(n) ( (fixupOrderArray == NULL) ? (n) : fixupOrderArray[n] )
	int startci = 0;
	if ( index >= numd ) {
		return 1; // success
	}
	if ( index > 0 ) {
		startci = (ci[fixupOrder(index-1)] + 1) % numStdColors;
	}
	for ( int cia = 0; cia < numStdColors; cia++ ) {
		POPTYPE thisc;
		int good;
		ci[fixupOrder(index)] = thisc = (startci + cia) % numStdColors;
		good = 1;
		// test this color pick against neighbor's colors
		for ( int i = 0; i < adjlen*2; i++ ) {
			if ( adjacency[i] == fixupOrder(index) ) {
				int oi;
				oi = adjacency[i ^ 1];
				if ( ci[oi] == thisc ) {
					good = 0;
					break;
				}
			}
		}
		if ( good ) {
			good = recolorFixup( adjacency, adjlen, ci, numd, index + 1, fixupOrderArray );
			if ( good ) {
				// it got to the end, all is well.
				return good;
			}
		}
	}
	ci[fixupOrder(index)] = (POPTYPE)-1;
	return 0;
}

int needsRecolor( POPTYPE* adjacency, int adjlen ) {
	for ( int i = 0; i < adjlen; i++ ) {
	const unsigned char* ca;
	const unsigned char* cb;
	ca = colors + ((adjacency[i*2  ] % numColors) * 3);
	cb = colors + ((adjacency[i*2+1] % numColors) * 3);
	if ( adjacency[i*2  ] == NODISTRICT || adjacency[i*2+1] == NODISTRICT ) {
	    continue;
	}
	if ( ca == cb ) {
	    fprintf(stderr,"dist %d adj %d, mod %d hash to same color\n", adjacency[i*2  ], adjacency[i*2+1], numColors );
	    return 1;
	}
	if ( ca[0] == cb[0] && ca[1] == cb[1] && ca[2] == cb[2] ) {
	    fprintf(stderr,"dist %d adj %d have same color (%02x,%02x,%02x)==(%02x,%02x,%02x)\n", adjacency[i*2  ], adjacency[i*2+1], ca[0], ca[1], ca[2], cb[0], cb[1], cb[2] );
	    return 1;
	}
	}
	return 0;
}

/*!
	winner POPTYPE[wlen]
	adjacency POPTYPE[adjlen*2]	a,b pairs
	*/
void recolorDists( POPTYPE* adjacency, int adjlen, int numd, POPTYPE* renumber ) {
	POPTYPE* ci;
	unsigned char* mc = (unsigned char*)colors;
#if 0
	static int delay = 10;
	if ( ! delay-- ) {
		fprintf(stderr,"adjacency:\n");
		for ( int i = 0; i < adjlen; i++ ) {
			fprintf(stderr,"%d %d\n", adjacency[i*2], adjacency[i*2+1] );
		}
		exit(1);
	}
#endif
	if ( numd != numColors || mc == stdColors ) {
		if ( mc != stdColors ) {
			free( mc );
			colors = NULL;
		}
		mc = (unsigned char*)malloc( sizeof(unsigned char) * 3 * numd );
		numColors = numd;
	} else if ( ! needsRecolor( adjacency, adjlen ) ) {
	// don't need to recolor
	return;
	}
	ci = (POPTYPE*)malloc( sizeof(POPTYPE) * numd );
	assert(ci != NULL);
	memset( ci, -1, sizeof(POPTYPE) * numd );
#if 0
	POPTYPE* fixupOrder = ci = (POPTYPE*)malloc( sizeof(POPTYPE) * numd );
	for ( int i = 0; i < numd; i++ ) {
		fixupOrder[i] = i;
	}
	if ( renumber != NULL ) {
		int notdone = 1;
		for ( int i = 0; i < numd; i++ ) {
			fprintf(stderr,"r %d -> %d\n", i, renumber[i] );
		}
		while ( notdone ) {
			notdone = 0;
			for ( int i = 0; i < numd-1; i++ ) {
				if ( renumber[fixupOrder[i]] > renumber[fixupOrder[i+1]] ) {
					POPTYPE td;
					td = fixupOrder[i];
					fixupOrder[i] = fixupOrder[i+1];
					fixupOrder[i+1] = td;
					notdone = 1;
				}
			}
		}
		for ( int i = 0; i < numd; i++ ) {
			fprintf(stderr,"f %d -> %d\n", i, fixupOrder[i] );
		}
	}
#endif
	if ( ! recolorFixup( adjacency, adjlen, ci, numd, 0, renumber ) ) {
		fprintf(stderr,"fixup failed to recolor\n");
		// fixup failed, set in order
		if ( renumber != NULL ) {
			for ( int i = 0; i < numd; i++ ) {
				ci[i] = renumber[i] % numStdColors;
			}
		} else {
			for ( int i = 0; i < numd; i++ ) {
				ci[i] = i % numStdColors;
			}
		}
	}
	for ( int i = 0; i < numd; i++ ) {
		unsigned char* cc;
		const unsigned char* sc;
		cc = mc + (i * 3);
		sc = stdColors + (ci[i] * 3);
		cc[0] = sc[0];
		cc[1] = sc[1];
		cc[2] = sc[2];
	}
	colors = mc;
#if 0
	fprintf(stderr,"ci:\n");
	for ( int i = 0; i < numd; i++ ) {
	const unsigned char* ca;
	ca = colors + ((i % numColors) * 3);
	fprintf(stderr,"ci[%d]\t%d\t(%02x,%02x,%02x)\n", i, ci[i], ca[0], ca[1], ca[2] );
	}
#endif
	if ( needsRecolor( adjacency, adjlen ) ) {
		fprintf(stderr,"fixup claims to have succeeded but we still need recolor\n");
		fprintf(stderr,"adjacency:\n");
		for ( int i = 0; i < adjlen; i++ ) {
			fprintf(stderr,"%d %d\n", adjacency[i*2], adjacency[i*2+1] );
		}
		fprintf(stderr,"ci:\n");
		for ( int i = 0; i < numd; i++ ) {
			const unsigned char* ca;
			ca = colors + ((i % numColors) * 3);
			fprintf(stderr,"ci[%d]\t%d\t(%02x,%02x,%02x)\n", i, ci[i], ca[0], ca[1], ca[2] );
		}
		exit(1);
	}
	free( ci );
}

void printColoring( FILE* out ) {
	for ( int i = 0; i < numColors; i++ ) {
		const unsigned char* ca;
		ca = colors + (i * 3);
		fprintf(out,"%02x %02x %02x\n", ca[0], ca[1], ca[2] );
	}
	fflush(out);
}

#define MAXNC 300
void readColoring( FILE* fin ) {
	unsigned char* nc = (unsigned char*)malloc(MAXNC*3);
	int i = 0;
	for ( i = 0; i < MAXNC; i++ ) {
		unsigned char* ca;
		int err;
		ca = nc + (i * 3);
		unsigned int a,b,c;
		err = fscanf(fin,"%02x %02x %02x\n", &a, &b, &c );
		if ( err < 3 ) {
			break;
		}
		ca[0] = a; ca[1] = b; ca[2] = c;
	}
	if ( i != numColors || colors == stdColors ) {
		if ( colors != stdColors ) {
			free( (void*)colors );
		}
		colors = (unsigned char*)malloc( sizeof(unsigned char) * 3 * i );
		assert(colors != NULL);
		numColors = i;
	}
	memcpy( (void*)colors, nc, sizeof(unsigned char) * 3 * i );
}
#endif /* WITH_PNG */
