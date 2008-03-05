#import "SolverGLView.h"
#include <OpenGL/gl.h>

void glDrawSolver( void* s );

static int notfirst = 0;

@implementation SolverGLView
- (void)drawRect:(NSRect) bound {
	if ( ! notfirst ) {
		NSOpenGLContext* oglc;
		notfirst = 1;
		oglc = [self openGLContext];
		printf("oglc = %p\n", oglc );
		if (oglc != nil) {
			NSOpenGLPixelBuffer* pb;
			pb = [oglc pixelBuffer];
			printf("NSOpenGLPixelBuffer %p\n", pb );
			if ( pb != nil ) {
				printf("NSOpenGLPixelBuffer %s\n", [[pb description] cString] );
			}
		}
	}
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	[sovm drawGL];
	glFlush();
}
- (void)reshape {
    NSRect bounds = [self bounds];
    double rat, fovy;
	[sovm setViewportRatio:(bounds.size.width / bounds.size.height)];
	//printf("viewportRatio %f (%f/%f)\n", ((bounds.size.width * 1.0) / bounds.size.height), bounds.size.width, bounds.size.height );
    glViewport( 0, 0, bounds.size.width, bounds.size.height );
}	
@end
