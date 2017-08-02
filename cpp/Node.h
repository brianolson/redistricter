#ifndef NODE_H
#define NODE_H

#include "config.h"

class Node {
public:
	int* neighbors;
	int numneighbors;
	// a node's "order" is the number of neighbors it has of the same district
	int order;
	
	int calcOrder( Node* nodes, POPTYPE* pit, int n ) {
		POPTYPE ndist = pit[n];
		order = 0;
		for ( int i = 0; i < numneighbors; i++ ) {
			if ( pit[neighbors[i]] == ndist ) {
				order++;
			}
		}
		return order;
	}
	bool isEdge( Node* nodes, POPTYPE* pit, int n ) {
		POPTYPE ndist = pit[n];
		for ( int i = 0; i < numneighbors; i++ ) {
			if ( pit[neighbors[i]] != ndist ) {
				return true;
			}
		}
		return false;
	}
	inline bool isNeighbor( int n ) {
		for ( int i = 0; i < numneighbors; i++ ) {
			if ( neighbors[i] == n ) {
				return true;
			}
		}
		return false;
	}
};

#include "Solver.h"



#endif
