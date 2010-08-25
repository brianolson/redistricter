#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "District2.h"
#include "Bitmap.h"
#include "Node.h"
#include "GeoData.h"
#include "tiger/mmaped.h"

static const char usage[] =
"usage: linkfixup [-o file][-p file][--have-protobuf][Solver args]\n"
#if HAVE_PROTOBUF
"  -p file   write out new protobuf file\n"
#endif
"  -o file   write out old '.gbin' format file\n"
"  --have-protobuf exit success if -p available, failure otherwise.\n"
#if HAVE_PROTOBUF
"\nthis linkfixup was compiled with protobuf support\n"
#else
"\nno protobuf support.\n"
#endif
;

int _aei( const char** argv, int argc, int i, const char* n ) {
	if ( i >= argc ) {
		fprintf(stderr,"expected argument but ran out after \"%s\"\n", argv[argc] );
		exit(1);
	}
	return ! strcmp( argv[i], n );
}
#define aei( n ) _aei( argv, argc, i, (n) )

class newedge {
public:
	int32_t a, b;
	newedge* next;
	
	newedge( int32_t i, int32_t j, newedge* ni = NULL ) : a( i ), b( j ), next( ni ) {}
};
newedge* neroot = NULL;
int necount = 0;

Node* initNodesFromLinksFile( GeoData* gd, const char* inputname );

int main( int argc, char** argv ) {
	Solver sov;
	int i, nargc;
	const char* foname = NULL;
#if HAVE_PROTOBUF
	const char* poname = NULL;
#endif
	
	nargc=1;
	
	for ( i = 1; i < argc; i++ ) {
		if ( ! strcmp( argv[i], "-o" ) ) {
			i++;
			foname = argv[i];
#if HAVE_PROTOBUF
		} else if ( ! strcmp( argv[i], "-p" ) ) {
			i++;
			poname = argv[i];
		} else if ( ! strcmp( argv[i], "--have-protobuf" ) ) {
			exit(0);
#else
		} else if ( ! strcmp( argv[i], "--have-protobuf" ) ) {
			exit(1);
#endif
		} else if ( (!strcmp( argv[i], "--help" )) ||
			    (!strcmp( argv[i], "-help" )) ||
			    (!strcmp( argv[i], "-h" )) ) {
			fputs(usage, stdout);
			exit(0);
		} else {
			argv[nargc] = argv[i];
			nargc++;
		}
	}
	argv[nargc]=NULL;
	int argcout = sov.handleArgs( nargc, argv );
	if (argcout != 1) {
		fprintf( stderr, "%s: bogus arg \"%s\"\n", argv[0], argv[1] );
		fputs( usage, stderr );
		fputs( Solver::argHelp, stderr );
		exit(1);
		return 1;
	}

#if HAVE_PROTOBUF
	if (( foname == NULL ) && ( poname == NULL ))
#else
	if ( foname == NULL )
#endif
	{
		fprintf(stderr,"useless linkfixup, null foname\n");
		exit(1);
	}
	sov.load();
	sov.initNodes();

#if 0
	GeoData* gd = gdopenf( (char*)finame );
	fprintf(stderr,"gd opened \"%s\"\n", finame);
	gd->load();
	fprintf(stderr,"gd->load done\n");
	Node* nodes = NULL;
	nodes = initNodesFromLinksFile( gd, finame );
	fprintf(stderr,"nodes load done\n");
#else
	GeoData* gd = sov.gd;
	Node* nodes = sov.nodes;
#endif

	int numPoints = gd->numPoints;
	Bitmap hit(numPoints);
	hit.zero();
	int* bfsearchq = new int[numPoints];
	int bfin = 0;
	int bfout = 0;
	
	bfsearchq[0] = 0;
	hit.set(0);
	bfin = 1;
	
#define modinc(n) do { (n) = ((n + 1) % numPoints); } while ( 0 )
	int nohitcount;
	int mini;
	int minj;
	double mind;

	do {
		while ( bfin != bfout ) {
			int nn = bfsearchq[bfout];
			Node* n = nodes + nn;
			modinc( bfout );
			for ( int ni = 0; ni < n->numneighbors; ni++ ) {
				int nin;
				nin = n->neighbors[ni];
				if ( ! hit.test(nin) ) {
					bfsearchq[bfin] = nin;
					modinc( bfin );
					hit.set( nin );
				}
			}
		}
		
		mini = -1;
		minj = -1;
		mind = 1000000000.0;
		nohitcount = 0;
		
		for ( int i = 0; i < numPoints; i++ ) {
			if ( ! hit.test(i) ) {
				for ( int j = 0; j < numPoints; j++ ) {
					if ( hit.test(j) ) {
						double d, dx, dy;
						dx = gd->pos[i*2] - gd->pos[j*2];
						dy = gd->pos[i*2 + 1] - gd->pos[j*2 + 1];
						d = sqrt( (dx*dx) + (dy*dy) );
						if ( d < mind ) {
							mini = i;
							minj = j;
							mind = d;
						}
					}
				}
				nohitcount++;
			}
		}
		if ( necount == 0 && nohitcount == 0 ) {
			printf("none unreachable.\n");
			break;
			//exit(0);
		}
		if ( nohitcount == 0 ) {
			break;
		}
		printf("%d not reachable from 0\n", nohitcount);
		printf("add link: %d,%d\n%013llu%013llu\n", mini, minj, gd->ubidOfIndex(mini), gd->ubidOfIndex(minj) );
		neroot = new newedge( mini, minj, neroot );
		necount++;
		
		bfsearchq[0] = mini;
		bfin = 1;
		bfout = 0;
		hit.set( mini );
	} while ( 1 );
	
	if ( necount > 0 ) {
		printf("writing %d new links to \"%s\"\n", necount, foname );
		int32_t* newe = new int32_t[(sov.numEdges + necount) * 2];
		memcpy( newe, sov.edgeData, sizeof(int32_t)*(sov.numEdges * 2) );
		delete [] sov.edgeData;
		sov.edgeData = newe;
		while ( neroot != NULL ) {
			newe[sov.numEdges*2] = neroot->a;
			newe[sov.numEdges*2 + 1] = neroot->b;
			neroot = neroot->next;
			sov.numEdges++;
		}
	}
	if ( foname != NULL ) {
		sov.writeBin(foname);
	}
#if HAVE_PROTOBUF
	if ( poname != NULL ) {
		sov.writeProtobuf(poname);
	}
#endif	

	delete [] bfsearchq;

	return 0;
}

Node* initNodesFromLinksFile( GeoData* gd, const char* inputname ) {
	int i, j;
	int maxneighbors = 0;
	int numPoints = gd->numPoints;
	Node* nodes = new Node[numPoints];
	for ( i = 0; i < numPoints; i++ ) {
		nodes[i].numneighbors = 0;
	}
	// read edges from edge file
	char* linkFileName = strdup( inputname );
	assert(linkFileName != NULL);
	{
		size_t nlen = strlen( linkFileName ) + 8;
		linkFileName = (char*)realloc( linkFileName, nlen );
		assert(linkFileName != NULL);
	}
	strcat( linkFileName, ".links" );
	mmaped linksFile;
	linksFile.open( linkFileName );
#define sizeof_linkLine 27
	int numEdges = linksFile.sb.st_size / sizeof_linkLine;
	long* edgeData = new long[numEdges*2];
	char buf[14];
	buf[13] = '\0';
	j = 0;
	for ( i = 0 ; i < numEdges; i++ ) {
		uint64_t tubid;
		memcpy( buf, ((caddr_t)linksFile.data) + sizeof_linkLine*i, 13 );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2  ] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2  ] < 0 ) {
			printf("ubid %lld => index %ld\n", tubid, edgeData[j*2] );
			continue;
		}
		memcpy( buf, ((caddr_t)linksFile.data) + sizeof_linkLine*i + 13, 13 );
		tubid = strtoull( buf, NULL, 10 );
		edgeData[j*2+1] = gd->indexOfUbid( tubid );
		if ( edgeData[j*2+1] < 0 ) {
			printf("ubid %lld => index %ld\n", tubid, edgeData[j*2+1] );
			continue;
		}
		//printf("ubid %lld => index %d\n", tubid, edgeData[i*2] );
		nodes[edgeData[j*2  ]].numneighbors++;
		//printf("ubid %lld => index %d\n", tubid, edgeData[i*2+1] );
		nodes[edgeData[j*2+1]].numneighbors++;
		j++;
	}
	numEdges = j;
	linksFile.close();
	free( linkFileName );
	// allocate all the space
	int* allneigh = new int[numEdges * 2]; // if you care, "delete [] nodes[0].neighbors;" somewhere
	int npos = 0;
	// give space to each node as counted above
	for ( i = 0; i < numPoints; i++ ) {
		Node* cur;
		cur = nodes + i;
		cur->neighbors = allneigh + npos;
		if ( cur->numneighbors > maxneighbors ) {
			maxneighbors = cur->numneighbors;
		}
		npos += cur->numneighbors;
		cur->numneighbors = 0;
	}
	// copy edges into nodes
	for ( j = 0; j < numEdges; j++ ) {
		int ea, eb;
		Node* na;
		Node* nb;
		ea = edgeData[j*2];
		eb = edgeData[j*2 + 1];
		na = nodes + ea;
		nb = nodes + eb;
		na->neighbors[na->numneighbors] = eb;
		na->numneighbors++;
		nb->neighbors[nb->numneighbors] = ea;
		nb->numneighbors++;
	}
	delete [] edgeData;
	return nodes;
}
