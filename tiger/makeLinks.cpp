#include "record1.h"
#include "mmaped.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ext/hash_set>

class blocklink {
public:
	uint64_t ubida, ubidb;
	blocklink( uint64_t a, uint64_t b ) {
		if ( a < b ) {
			ubida = a;
			ubidb = b;
		} else {
			ubida = b;
			ubidb = a;
		}
	}
};
class halflink {
public:
	uint64_t ubid;
	uint32_t tlid;
	halflink( uint64_t u, uint32_t t ) : ubid( u ) , tlid( t ) {}
};

// C++ is fugly
namespace __gnu_cxx {
	
template<> struct hash<blocklink> {
	size_t operator()( const blocklink& x ) const {
		if ( sizeof(uint64_t) > sizeof(size_t) ) {
			return (x.ubida && 0xffffffff) + (x.ubida >> 32) + (x.ubidb & 0xffffffff) + (x.ubidb >> 32);
		} else {
			return x.ubida + x.ubidb;
		}
	}
};

template<> struct hash<halflink> {
	size_t operator()( const halflink& x ) const {
		return x.tlid;
	}
};

}

namespace std {
	template<> struct equal_to<blocklink> {
		bool operator()( const blocklink& a, const blocklink& b ) const {
			return a.ubida == b.ubida && a.ubidb == b.ubidb;
		}
	};
	template<> struct equal_to<halflink> {
		bool operator()( const halflink& a, const halflink& b ) const {
			return a.tlid == b.tlid;
		}
	};
}

using namespace __gnu_cxx;

//template class hash_set<blocklink>;

hash_set<blocklink> they;

hash_set<halflink> halves;

class source {
public:
	int len;

	const char* filename;
	mmaped m;
	record1 r;
	
	source* next;
	source* prev;
	
	source( const char* f, source* n ) : len( 0 ), filename( f ), next( n ) {
		if ( next != NULL ) {
			prev = next->prev;
			next->prev = this;
			if ( prev != NULL ) {
				prev->next = this;
			}
		} else {
			prev = NULL;
		}
	}
};

#define memcpy( dst, src, len ) my_inline_memcpy( dst, src, len )
#define uint8_t unsigned char
inline void my_inline_memcpy( void* dst, const void* src, size_t len ) {
	uint8_t* du = (uint8_t*)dst;
	const uint8_t* su = (const uint8_t*)src;
	while ( len > 0 ) {
		*du = *su;
		du++;
		su++;
		len--;
	}
}

inline uint64_t ubid( unsigned long county, unsigned long tract, unsigned long block ) {
	uint64_t toret = county;
	assert( county <= 999 );
	assert( tract <= 999999 );
	assert( block <= 9999 );
	toret *= 1000000;
	toret += tract;
	toret *= 10000;
	toret += block;
	return toret;
}

void trypair( uint64_t a, uint64_t b ) {
	blocklink tbl( a, b );
	if ( they.count( tbl ) == 0 ) {
		they.insert( tbl );
	}
}

void tryhalf( uint64_t ubidr, uint32_t tlid ) {
	halflink nh( ubidr, tlid );
	hash_set<halflink>::iterator hi = halves.find( nh );
	if ( hi != halves.end() ) {
		halflink oh = *hi;
//		printf("found old half %u\n", oh.tlid );
		halves.erase( nh );
		if ( ubidr == oh.ubid ) {
			fprintf(stderr,"weird split, tlid=%u ubid=%013llu\n", tlid, ubidr );
		} else {
			trypair( ubidr, oh.ubid );
		}
	} else {
		halves.insert( nh );
	}
}

int main( int argc, const char** argv ) {
	source* root = NULL;
	const char* outname = NULL;
	FILE* out = NULL;

	for ( int i = 1; i < argc; i++ ) {
		if ( !strcmp( argv[i], "-o" ) ) {
			i++;
			outname = argv[i];
			out = fopen( outname, "wb" );
			if ( out == NULL ) {
				perror("fopen");
				exit(1);
			}
		} else {
			root = new source( argv[i], root );
		}
	}
	if ( out == NULL ) {
		printf("no output specified\n");
		exit(1);
		//out = stdout;
	}
	
	source* cur;
	cur = root;
	long recordsin = 0;
	long recordsout = 0;
	while ( cur != NULL ) {
		int err;
                fprintf(stderr,"%s: starting\n", cur->filename);
		err = cur->m.open( cur->filename );
		if ( err != 0 ) {
			perror("mmaped.open");
			exit(1);
		}
		cur->len = cur->m.sb.st_size / record1::size;
		cur->r.base = (uint8_t*)(cur->m.data);
		recordsin += cur->len;
		
		for ( int i = 0; i < cur->len; i++ ) {
			uint32_t tlid;
			uint64_t ubidl, ubidr;
			int nol, nor;
			tlid = cur->r.TLID_longValue(i);
			if ( cur->r.STATEL(i)[0] == ' ' || cur->r.COUNTYL(i)[0] == ' ' ||
				 cur->r.TRACTL(i)[0] == ' ' || cur->r.BLOCKL(i)[0] == ' ' ) {
				// left side empty
				nol = 1;
				ubidl = 0;
			} else {
				nol = 0;
				ubidl = ubid( cur->r.COUNTYL_longValue(i), cur->r.TRACTL_longValue(i), cur->r.BLOCKL_longValue(i) );
			}
			if ( cur->r.STATER(i)[0] == ' ' || cur->r.COUNTYR(i)[0] == ' ' ||
				 cur->r.TRACTR(i)[0] == ' ' || cur->r.BLOCKR(i)[0] == ' ' ) {
				// right side empty
				nor = 1;
				ubidr = 0;
			} else {
				nor = 0;
				ubidr = ubid( cur->r.COUNTYR_longValue(i), cur->r.TRACTR_longValue(i), cur->r.BLOCKR_longValue(i) );
			}
			if ( nol ) {
				if ( nor ) {
					printf("error, %s:%d has neither left nor right\n", cur->filename, i );
				} else {
					tryhalf( ubidr, tlid );
				}
			} else {
				if ( nor ) {
					tryhalf( ubidl, tlid );
				} else if ( ubidl != ubidr ) {
					trypair( ubidl, ubidr );
				}
			}
		}
		
		cur->m.close();
		cur = cur->next;
	}
	{
		hash_set<blocklink>::iterator bi = they.begin();
		hash_set<blocklink>::iterator end = they.end();
		while ( bi != end ) {
			fprintf(out,"%013llu%013llu\n",bi->ubida,bi->ubidb);
			bi++;
		}
		fflush( out );
	}
	recordsout = they.size();
	printf("%ld records in\n%ld links out\n%lu unpaired halves\n", recordsin, recordsout, (long unsigned int)(halves.size()) );
	fclose( out );
}
