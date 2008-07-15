/* SolverM */

#import <Cocoa/Cocoa.h>
#include <pthread.h>

@class SolverGLView;
@class SolverPixView;

@interface SolverM : NSObject
{
	IBOutlet NSView *view;
	IBOutlet NSTextField *stats;

	IBOutlet NSControl *_edgeRelativeDistanceFactor;
	IBOutlet NSControl *_odEdgeRelativeDistanceFactor;
	IBOutlet NSControl *_popRatioFactor;
	
	IBOutlet NSControl *zoomBox;
	IBOutlet NSControl *dcxBox;
	IBOutlet NSControl *dcyBox;

	IBOutlet NSPopUpButton *nodeColoringMenu;
	IBOutlet NSPopUpButton *debugDistrictMenu;
	
	IBOutlet NSTextView *extraText;
	IBOutlet NSWindow *extraTextWindow;

	void* cppSolver;
	void* cppMapDrawer;

	BOOL isPixView;
#if 0
#define USE_NS_THREAD 1
	//NSThread runThread;
#else
#define USE_NS_THREAD 0
	pthread_t runThread;
	pthread_mutex_t runControlM;
	pthread_cond_t runControl;
	enum {
		kSolverStopped = 0,
		kSolverRunning = 1,
		kSolverStepping = 2
	} runMode;
	pthread_mutex_t sovLock;
#endif
}
- (void)awakeFromNib;
- (IBAction)setRunning:(id)sender;
- (IBAction)run:(id)sender;
- (IBAction)step:(id)sender;
- (int)stepDisplay:(BOOL)needsDisplay;
- (IBAction)stop:(id)sender;
- (IBAction)writePNG:(id)sender;
- (IBAction)startSavePNG:(id)sender;
- (void)savePNGDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;
- (IBAction)startSaveDists:(id)sender;
- (void)saveDistsDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;
- (IBAction)startSaveSolution:(id)sender;
- (void)saveSolutionDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo;
- (IBAction)startOpenData:(id)sender;
- (void)openDataDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;

- (IBAction)nudgeViewUp:(id)sender;
- (IBAction)nudgeViewLeft:(id)sender;
- (IBAction)nudgeViewRight:(id)sender;
- (IBAction)nudgeViewDown:(id)sender;
- (IBAction)zoomIn:(id)sender;
- (IBAction)zoomOut:(id)sender;
- (IBAction)zoomAll:(id)sender;
- (IBAction)setShowLinks:(id)sender;

- (IBAction)setEdgeRelativeDistanceFactor:(id)sender;
- (IBAction)setOdEdgeRelativeDistanceFactor:(id)sender;
- (IBAction)setPopRatioFactor:(id)sender;

- (IBAction)setZoom:(id)sender;
- (IBAction)setDcx:(id)sender;
- (IBAction)setDcy:(id)sender;
- (IBAction)goZXY:(id)sender;

- (IBAction)setNodeColoring:(id)sender;
- (IBAction)setDebugDistrict:(id)sender;

- (void)drawGL;
- (void)drawPix:(NSRect) rect;
- (void)setViewportRatio:(double) vr;

- (void*)runThreadProc;

- (void)setupDebugDistrictMenu;
- (void)setupNodeColoringMenu;

- (void)updateStatsPane;

/* returns NULL if there isn't one or if it isn't visible */
- (NSTextView*)extraTextIfAppropriate;

/* fill extraText with debug info */
- (void)setExtraText;

/* NSKeyValueCoding */
- (id)valueForKey:(NSString *)key;
- (id)valueForKeyPath:(NSString *)keyPath;
- (id)valueForUndefinedKey:(NSString *)key;

@end
