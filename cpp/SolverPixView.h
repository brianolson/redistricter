//
//  SolverPixView.h
//  guidistricter
//
//  Created by Brian Olson on 7/14/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "SolverM.h"

@interface SolverPixView : NSView {
	IBOutlet SolverM *sovm;
}
- (void)drawRect:(NSRect) rect;
@end
