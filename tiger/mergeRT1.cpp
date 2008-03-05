#include "record1.h"
#include "mmaped.h"
#include <stdio.h>
#include <string.h>

class source {
public:
	long value;
	int index;
	int len;

	const char* filename;
	mmaped m;
	record1 r;
	
	source* next;
	source* prev;
	
	source( const char* f, source* n ) : value( 0 ), index( 0 ), len( 0 ), filename( f ), next( n ) {
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
	
	inline long getValue() {
		//return value = r.TLID_longValue( index );
		return value = tlidSortIndecies[index*2];
	}
	inline const char* record( int index ) const {
		return r.record( tlidSortIndecies[index*2+1] );
	}
	
	long* tlidSortIndecies;
	
	void sortOnTLID();
};

#if 0
long source::getValue() {
	return value = r.TLID_longValue( index );
}
#endif

static int sotC( const void* a, const void* b ) {
	return (int)(((long*)a)[0] - ((long*)b)[0]);
}

void source::sortOnTLID() {
	// quicksort
	if ( tlidSortIndecies == NULL ) {
		tlidSortIndecies = new long[len*2];
	}
	int i;
	for ( i = 0; i < len; i++ ) {
		tlidSortIndecies[i*2] = r.TLID_longValue( i );
		tlidSortIndecies[i*2 + 1] = i;
	}
	// quicksort
	qsort( tlidSortIndecies, len, sizeof( long ) * 2, sotC );
#if 0
	for ( i = 0; i < len; i++ ) {
		printf("%d: %ld\n", i, tlidSortIndecies[i*2] );
	}
#endif
}

source* stubbleSort( source* root ) {
	if ( root == NULL || root->next == NULL ) {
		return root;
	}
	source* cur;
	bool notdone = true;
	while ( notdone ) {
		cur = root;
		notdone = false;
		while ( cur != NULL && cur->next != NULL ) {
			if ( cur->next->value < cur->value ) {
				// a cur b c
				// a b cur c
				source* a = cur->prev;
				source* b = cur->next;
				source* c = b->next;
				if ( a ) {
					a->next = b;
				}
				b->prev = a;
				b->next = cur;
				cur->prev = b;
				cur->next = c;
				if ( c ) {
					c->prev = cur;
				}
				if ( cur == root ) {
					root = b;
				}
				notdone = true;
			} else {
				cur = cur->next;
			}
		}
	}
	return root;
}

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

int rt1MergeByFields( char* out, const char* a, const char* b ) {
	// -1 = left, 0 = both/neither, 1 = right
	int apos = 0, bpos = 0;
	if ( a[182] == ' ' && a[186] != ' ' ) {
		apos = 1;
	} else if ( a[182] != ' ' && a[186] == ' ' ) {
		apos = -1;
	}
	if ( b[182] == ' ' && b[186] != ' ' ) {
		bpos = 1;
	} else if ( b[182] != ' ' && b[186] == ' ' ) {
		bpos = -1;
	}
	if ( apos == 0 && bpos == 0 ) {
		fprintf(stderr, "both are both or neither\n" );
		return -1;
	}
	bool aintoleft = true;
	if ( (apos == -1) || (bpos == 1) ) {
		aintoleft = true;
	} else {
		aintoleft = false;
	}
	//printf("%10.10s\tapos %d bpos %d aintoleft %d\n", a + 6, apos, bpos, aintoleft );
	memcpy( out, a, 58 );
	if ( aintoleft ) {
		if ( apos <= 0 ) {
			memcpy( out + 58, a + 58, 22 );		// FRADDRL,TOADDL
			memcpy( out + 102, a + 102, 2 );	// FRIADDL,TOIADDL
			memcpy( out + 106, a + 106, 5 );	// ZIPL
			memcpy( out + 116, a + 116, 5 );	// AIANHHFPL
			memcpy( out + 126, a + 126, 1 );	// AIHHTLIL
			memcpy( out + 130, a + 130, 2 );	// STATEL
			memcpy( out + 134, a + 134, 3 );	// COUNTYL
			memcpy( out + 140, a + 140, 5 );	// COUSUBL
			memcpy( out + 150, a + 150, 5 );	// SUBMCDL
			memcpy( out + 160, a + 160, 5 );	// PLACEL
			memcpy( out + 170, a + 170, 6 );	// TRACTL
			memcpy( out + 182, a + 182, 4 );	// BLOCKL
		} else {
			memcpy( out + 58, a + 80, 22 );		// FRADDRR,TOADDR
			memcpy( out + 102, a + 104, 2 );	// FRIADDR,TOIADDR
			memcpy( out + 106, a + 111, 5 );	// ZIPR
			memcpy( out + 116, a + 121, 5 );	// AIANHHFPR
			memcpy( out + 126, a + 127, 1 );	// AIHHTLIR
			memcpy( out + 130, a + 132, 2 );	// STATER
			memcpy( out + 134, a + 137, 3 );	// COUNTYR
			memcpy( out + 140, a + 145, 5 );	// COUSUBR
			memcpy( out + 150, a + 155, 5 );	// SUBMCDR
			memcpy( out + 160, a + 165, 5 );	// PLACER
			memcpy( out + 170, a + 176, 6 );	// TRACTR
			memcpy( out + 182, a + 186, 4 );	// BLOCKR
		}
		if ( bpos <= 0 ) {
			memcpy( out + 80, b + 58, 22 );		// FRADDRL,TOADDL
			memcpy( out + 104, b + 102, 2 );	// FRIADDL,TOIADDL
			memcpy( out + 111, b + 106, 5 );	// ZIPL
			memcpy( out + 121, b + 116, 5 );	// AIANHHFPL
			memcpy( out + 127, b + 126, 1 );	// AIHHTLIL
			memcpy( out + 132, b + 130, 2 );	// STATEL
			memcpy( out + 137, b + 134, 3 );	// COUNTYL
			memcpy( out + 145, b + 140, 5 );	// COUSUBL
			memcpy( out + 155, b + 150, 5 );	// SUBMCDL
			memcpy( out + 165, b + 160, 5 );	// PLACEL
			memcpy( out + 176, b + 170, 6 );	// TRACTL
			memcpy( out + 186, b + 182, 4 );	// BLOCKL
		} else {
			memcpy( out + 80, b + 80, 22 );		// FRADDRR,TOADDR
			memcpy( out + 104, b + 104, 2 );	// FRIADDR,TOIADDR
			memcpy( out + 111, b + 111, 5 );	// ZIPR
			memcpy( out + 121, b + 121, 5 );	// AIANHHFPR
			memcpy( out + 127, b + 127, 1 );	// AIHHTLIR
			memcpy( out + 132, b + 132, 2 );	// STATER
			memcpy( out + 137, b + 137, 3 );	// COUNTYR
			memcpy( out + 145, b + 145, 5 );	// COUSUBR
			memcpy( out + 155, b + 155, 5 );	// SUBMCDR
			memcpy( out + 165, b + 165, 5 );	// PLACER
			memcpy( out + 176, b + 176, 6 );	// TRACTR
			memcpy( out + 186, b + 186, 4 );	// BLOCKR
		}
	} else {
		if ( bpos <= 0 ) {
			memcpy( out + 58, b + 58, 22 );		// FRADDRL,TOADDL
			memcpy( out + 102, b + 102, 2 );	// FRIADDL,TOIADDL
			memcpy( out + 106, b + 106, 5 );	// ZIPL
			memcpy( out + 116, b + 116, 5 );	// AIANHHFPL
			memcpy( out + 126, b + 126, 1 );	// AIHHTLIL
			memcpy( out + 130, b + 130, 2 );	// STATEL
			memcpy( out + 134, b + 134, 3 );	// COUNTYL
			memcpy( out + 140, b + 140, 5 );	// COUSUBL
			memcpy( out + 150, b + 150, 5 );	// SUBMCDL
			memcpy( out + 160, b + 160, 5 );	// PLACEL
			memcpy( out + 170, b + 170, 6 );	// TRACTL
			memcpy( out + 182, b + 182, 4 );	// BLOCKL
		} else {
			memcpy( out + 58, b + 80, 22 );		// FRADDRR,TOADDR
			memcpy( out + 102, b + 104, 2 );	// FRIADDR,TOIADDR
			memcpy( out + 106, b + 111, 5 );	// ZIPR
			memcpy( out + 116, b + 121, 5 );	// AIANHHFPR
			memcpy( out + 126, b + 127, 1 );	// AIHHTLIR
			memcpy( out + 130, b + 132, 2 );	// STATER
			memcpy( out + 134, b + 137, 3 );	// COUNTYR
			memcpy( out + 140, b + 145, 5 );	// COUSUBR
			memcpy( out + 150, b + 155, 5 );	// SUBMCDR
			memcpy( out + 160, b + 165, 5 );	// PLACER
			memcpy( out + 170, b + 176, 6 );	// TRACTR
			memcpy( out + 182, b + 186, 4 );	// BLOCKR
		}
		if ( apos <= 0 ) {
			memcpy( out + 80, a + 58, 22 );		// FRADDRL,TOADDL
			memcpy( out + 104, a + 102, 2 );	// FRIADDL,TOIADDL
			memcpy( out + 111, a + 106, 5 );	// ZIPL
			memcpy( out + 121, a + 116, 5 );	// AIANHHFPL
			memcpy( out + 127, a + 126, 1 );	// AIHHTLIL
			memcpy( out + 132, a + 130, 2 );	// STATEL
			memcpy( out + 137, a + 134, 3 );	// COUNTYL
			memcpy( out + 145, a + 140, 5 );	// COUSUBL
			memcpy( out + 155, a + 150, 5 );	// SUBMCDL
			memcpy( out + 165, a + 160, 5 );	// PLACEL
			memcpy( out + 176, a + 170, 6 );	// TRACTL
			memcpy( out + 186, a + 182, 4 );	// BLOCKL
		} else {
			memcpy( out + 80, a + 80, 22 );		// FRADDRR,TOADDR
			memcpy( out + 104, a + 104, 2 );	// FRIADDR,TOIADDR
			memcpy( out + 111, a + 111, 5 );	// ZIPR
			memcpy( out + 121, a + 121, 5 );	// AIANHHFPR
			memcpy( out + 127, a + 127, 1 );	// AIHHTLIR
			memcpy( out + 132, a + 132, 2 );	// STATER
			memcpy( out + 137, a + 137, 3 );	// COUNTYR
			memcpy( out + 145, a + 145, 5 );	// COUSUBR
			memcpy( out + 155, a + 155, 5 );	// SUBMCDR
			memcpy( out + 165, a + 165, 5 );	// PLACER
			memcpy( out + 176, a + 176, 6 );	// TRACTR
			memcpy( out + 186, a + 186, 4 );	// BLOCKR
		}
	}
	memcpy( out + 190, a + 190, 40 );		// FRLONG..TOLAT\r\n
	return 0;
}

int rt1Merge( char* out, const char* a, const char* b ) {
	for ( int i = 0; i < record1::size; i++ ) {
		if ( a[i] == ' ' ) {
			out[i] = b[i];
		} else if ( b[i] == ' ' ) {
			out[i] = a[i];
		} else if ( a[i] != b[i] && i > 4 ) {
			int err = rt1MergeByFields( out, a, b );
			if ( err == 0 ) {
				return err;
			}
			fprintf(stderr,"record mis-merge, diff on non-space char at %d: '%c' != '%c'\n", i, a[i], b[i] );
			return err;
		} else {
			out[i] = a[i];
		}
	}
	return 0;
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
	
	source* done = NULL;
	source* cur;
	cur = root;
	long recordsin = 0;
	long recordsout = 0;
	while ( cur != NULL ) {
		int err;
		err = cur->m.open( cur->filename );
		if ( err != 0 ) {
			perror("mmaped.open");
			exit(1);
		}
		cur->len = cur->m.sb.st_size / record1::size;
		cur->r.base = (uint8_t*)(cur->m.data);
		cur->sortOnTLID();
		cur->getValue();
		printf("opened %s, %d records, first TLID %ld\n", cur->filename, cur->len, cur->value );
		recordsin += cur->len;
		cur = cur->next;
	}
	root = stubbleSort( root );
	char* mergeScratch = new char[record1::size];
//	printf("first record %ld in %s\n", root->value, root->filename );
	while ( root != NULL ) {
		if ( root->next != NULL && 0 ) {
			printf("%s[%d]\t%ld\t%s[%d]\t%ld\n", root->filename, root->index, root->value, root->next->filename, root->next->index, root->next->value );
		}
		if ( root->next != NULL && root->value == root->next->value ) {
			source* rn = root->next;
			const char* a;
			const char* b;
			a = root->record( root->index );
			b = rn->record( rn->index );
			if ( rt1Merge( mergeScratch, a, b ) != 0 ) {
				fprintf(stderr,"record mis-merge between %s and %s\n", root->filename, rn->filename );
				fwrite( a, 1, record1::size, stderr );
				fwrite( b, 1, record1::size, stderr );
				fwrite( a, 1, record1::size, out );
				fwrite( b, 1, record1::size, out );
				//exit(1);
				recordsout += 2;
			} else {
				fwrite( mergeScratch, 1, record1::size, out );
				recordsout++;
			}
			rn->index++;
			if ( rn->index >= rn->len ) {
				// root rn tr
				// root tr
				source* tr = rn->next;
				printf("finished %s, %ld records out\n", rn->filename, recordsout );
				rn->next = done;
				rn->prev = NULL;
				done = rn;
				if ( tr != NULL ) {
					tr->prev = root;
				}
				root->next = tr;
			} else {
				rn->getValue();
			}
		} else {
			fwrite( root->record( root->index ), 1, record1::size, out );
			recordsout++;
		}
		root->index++;
		if ( root->index >= root->len ) {
			source* tr = root->next;
			printf("finished %s, %ld records out\n", root->filename, recordsout );
			root->next = done;
			done = root;
			root = tr;
			if ( root == NULL ) {
				break;
			}
			root->prev = NULL;
		} else {
			root->getValue();
			root = stubbleSort( root );
		}
	}
	printf("%ld records in\n%ld records out\n", recordsin, recordsout );
	fclose( out );
	cur = done;
	while ( cur != NULL ) {
		cur->m.close();
		cur = cur->next;
	}
}
