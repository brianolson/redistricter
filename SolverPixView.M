//
//  SolverPixView.m
//  guidistricter
//
//  Created by Brian Olson on 7/14/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import "SolverPixView.h"
//#import <NSGraphics.h>
#include "MapDrawer.h"
#include "Solver.h"

@implementation SolverPixView

- (id)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        // Initialization code here.
    }
    return self;
}

- (void)drawRect:(NSRect)rect {
#if 0
	fprintf(stderr, "draw pix top=%f left=%f height=%f width=%f\n",
		rect.origin.y, rect.origin.x,
		rect.size.height, rect.size.width);
#endif
	//[sovm drawPix:rect];
	[sovm drawPix:[self bounds]];
}

@end
