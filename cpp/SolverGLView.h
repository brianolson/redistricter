/* SolverGLView */

#import <Cocoa/Cocoa.h>
#import "SolverM.h"

@interface SolverGLView : NSOpenGLView
{
	IBOutlet SolverM *sovm;
}
- (void)drawRect:(NSRect) bound;
- (void)reshape;
@end
