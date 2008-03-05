#import "SolverM.h"
#import "SolverGLView.h"
#include "Solver.h"
#include "GrabIntermediateStorage.h"

#define sov ((Solver*)cppSolver)

void* runThreadProc( void* );

extern "C" const char* getNodeColoringName(int index);
extern "C" void setNodeColoringByIndex(int index);
extern "C" void setDebugDistrictNumber(int index);

@implementation SolverM
- (void)awakeFromNib {
#if 1
	cppSolver = NULL;
#else
	//sov = new Solver();
	cppSolver = new Solver();
#if 0
	sov->inputname = "input_zcta5";
	sov->geoFact = openZCTA;
#else
	sov->inputname = "input.uf1";//"nmpb.uf1";
	sov->geoFact = openUf1;
#endif
	sov->districts = -10; // negative sign is a flag that means the value may be overriden by the file loaded
	sov->megaInit();
#endif

	runMode = kSolverStopped;
	pthread_mutex_init( &runControlM, NULL );
	pthread_cond_init( &runControl, NULL );
	pthread_mutex_init( &sovLock, NULL );
	pthread_create( &runThread, NULL, runThreadProc, self );
	if ( _edgeRelativeDistanceFactor ) {
		[_edgeRelativeDistanceFactor setDoubleValue: District2::edgeRelativeDistanceFactor];
	}
	if ( _odEdgeRelativeDistanceFactor ) {
		[_odEdgeRelativeDistanceFactor setDoubleValue: District2::odEdgeRelativeDistanceFactor];
	}
	if ( _popRatioFactor ) {
		[_popRatioFactor setDoubleValue: District2::popRatioFactor];
	}
	if ( zoomBox && sov ) {
		[zoomBox setDoubleValue: sov->zoom];
	}
	if ( dcxBox && sov ) {
		[dcxBox setDoubleValue: sov->dcx];
	}
	if ( dcyBox && sov ) {
		[dcyBox setDoubleValue: sov->dcy];
	}

	if ( extraText ) {
		[extraText setFont:[NSFont fontWithName:@"Monaco" size:10]];
	}
}

- (IBAction)setRunning:(id)sender {
	if ( [sender respondsToSelector:@selector(intValue)] ) {
		int running = [sender intValue];
		pthread_mutex_lock( &runControlM );
		runMode = (running != 0) ? kSolverRunning : kSolverStopped;
		pthread_cond_signal( &runControl );
		pthread_mutex_unlock( &runControlM );
	}
}

- (IBAction)run:(id)sender {
	pthread_mutex_lock( &runControlM );
	runMode = kSolverRunning;
	pthread_cond_signal( &runControl );
	pthread_mutex_unlock( &runControlM );
}

- (IBAction)step:(id)sender {
#if 01
	pthread_mutex_lock( &runControlM );
	runMode = kSolverStepping;
	pthread_cond_signal( &runControl );
	pthread_mutex_unlock( &runControlM );
#else	
	pthread_mutex_lock( &sovLock );
	sov->step();
	pthread_mutex_unlock( &sovLock );
	[view setNeedsDisplay:YES];
#endif
}

- (int)stepDisplay:(BOOL)needsDisplay {
	if ( sov == NULL ) return 0;
	int err;
	pthread_mutex_lock( &sovLock );
	err = sov->step();
	pthread_mutex_unlock( &sovLock );
	if ( err < 0 ) {
		needsDisplay = YES;
	}
	//fprintf(stderr,"SolverM.stepDisplay %d\n", needsDisplay);
	if ( needsDisplay ) {
		[view setNeedsDisplay:YES];
		[self updateStatsPane];
	}
	return err;
}

- (IBAction)stop:(id)sender {
	pthread_mutex_lock( &runControlM );
	runMode = kSolverStopped;
	pthread_cond_signal( &runControl );
	pthread_mutex_unlock( &runControlM );
}

- (void)drawGL {
	if ( sov == NULL ) return;
	//fprintf(stderr,"SolverM.drawGL\n");
	pthread_mutex_lock( &sovLock );
	sov->drawGL();
	pthread_mutex_unlock( &sovLock );
}

- (void)setViewportRatio:(double) vr {
	if ( sov == NULL ) return;
	sov->viewportRatio = vr;
}

- (IBAction)nudgeViewUp:(id)sender {
	if ( sov == NULL ) return;
	sov->nudgeUp();
	if ( dcyBox ) {
		[dcyBox setDoubleValue: sov->dcy];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)nudgeViewLeft:(id)sender {
	if ( sov == NULL ) return;
	sov->nudgeLeft();
	if ( dcxBox ) {
		[dcxBox setDoubleValue: sov->dcx];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)nudgeViewRight:(id)sender {
	if ( sov == NULL ) return;
	sov->nudgeRight();
	if ( dcxBox ) {
		[dcxBox setDoubleValue: sov->dcx];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)nudgeViewDown:(id)sender {
	if ( sov == NULL ) return;
	sov->nudgeDown();
	if ( dcyBox ) {
		[dcyBox setDoubleValue: sov->dcy];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)zoomIn:(id)sender {
	if ( sov == NULL ) return;
	sov->zoomIn();
	if ( zoomBox ) {
		[zoomBox setDoubleValue: sov->zoom];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)zoomOut:(id)sender {
	if ( sov == NULL ) return;
	sov->zoomOut();
	if ( zoomBox ) {
		[zoomBox setDoubleValue: sov->zoom];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)zoomAll:(id)sender {
	if ( sov == NULL ) return;
	sov->zoomAll();
	if ( zoomBox ) {
		[zoomBox setDoubleValue: sov->zoom];
	}
	if ( dcxBox ) {
		[dcxBox setDoubleValue: sov->dcx];
	}
	if ( dcyBox ) {
		[dcyBox setDoubleValue: sov->dcy];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)setShowLinks:(id)sender {
	if ( [sender respondsToSelector:@selector(intValue)] ) {
		if ( sov == NULL ) return;
		sov->showLinks = [sender intValue];
		//printf("setShowLinks = %d\n", sov->showLinks );
		[view setNeedsDisplay:YES];
	} else {
		printf("don't know what to do with sender %p\n", sender );
	}
}

#define trySetDoubleFromSender( var, update, sender ) 	if ( [sender respondsToSelector:@selector(doubleValue)] ) {\
	var = [sender doubleValue];\
	if ( update ) { [update setDoubleValue: var]; }\
} else {\
	printf( "%s from unkown sender %p\n", __FUNCTION__, sender ); }

- (IBAction)setEdgeRelativeDistanceFactor:(id)sender {
	if ( [sender respondsToSelector:@selector(doubleValue)] ) {
		District2::edgeRelativeDistanceFactor = [sender doubleValue];
		if ( _edgeRelativeDistanceFactor ) {
			[_edgeRelativeDistanceFactor setDoubleValue: District2::edgeRelativeDistanceFactor];
		}
	} else {
		printf("setEdgeRelativeDistanceFactor from unkown sender %p\n", sender );
	}
}
- (IBAction)setOdEdgeRelativeDistanceFactor:(id)sender {
	if ( [sender respondsToSelector:@selector(doubleValue)] ) {
		District2::odEdgeRelativeDistanceFactor = [sender doubleValue];
		if ( _odEdgeRelativeDistanceFactor ) {
			[_odEdgeRelativeDistanceFactor setDoubleValue: District2::odEdgeRelativeDistanceFactor];
		}
	} else {
		printf("setOdEdgeRelativeDistanceFactor from unkown sender %p\n", sender );
	}
}
- (IBAction)setPopRatioFactor:(id)sender {
	if ( [sender respondsToSelector:@selector(doubleValue)] ) {
		District2::popRatioFactor = [sender doubleValue];
		if ( _popRatioFactor ) {
			[_popRatioFactor setDoubleValue: District2::popRatioFactor];
		}
	} else {
		printf("setPopRatioFactor from unkown sender %p\n", sender );
	}
}
- (IBAction)setZoom:(id)sender {
	if ( sov == NULL ) return;
	trySetDoubleFromSender( sov->zoom, zoomBox, sender );
}
- (IBAction)setDcx:(id)sender {
	if ( sov == NULL ) return;
	trySetDoubleFromSender( sov->dcx, dcxBox, sender );
}
- (IBAction)setDcy:(id)sender {
	if ( sov == NULL ) return;
	trySetDoubleFromSender( sov->dcy, dcyBox, sender );
}
- (IBAction)goZXY:(id)sender {
	if ( sov == NULL ) return;
	if ( zoomBox ) {
		sov->zoom = [zoomBox doubleValue];
	}
	if ( dcxBox ) {
		sov->dcx = [dcxBox doubleValue];
	}
	if ( dcyBox ) {
		sov->dcy = [dcyBox doubleValue];
	}
	[view setNeedsDisplay:YES];
}

- (IBAction)setNodeColoring:(id)sender {
	int index = [sender indexOfSelectedItem];
	setNodeColoringByIndex( index );
	//fprintf(stderr,"SolverM.setNodeColoring %d\n", index);
	sov->lastGenDrawn = -1;
	[view setNeedsDisplay:YES];
}

- (IBAction)setDebugDistrict:(id)sender {
	if ( sov == NULL ) return;
	int index = [sender indexOfSelectedItem];
	sov->debugDistrictNumber = index - 1;
	//fprintf(stderr,"SolverM.setDebugDistrict %d\n", index);
	sov->lastGenDrawn = -1;
	[view setNeedsDisplay:YES];
}

- (IBAction)startSavePNG:(id)sender {
	if ( sov == NULL ) return;
#if WITH_PNG
	NSSavePanel* sheet = [NSSavePanel savePanel];
	[sheet beginSheetForDirectory:nil file:nil modalForWindow:[view window] modalDelegate:self didEndSelector:@selector(savePNGDidEnd:returnCode:contextInfo:) contextInfo:NULL];
#endif
}
- (void)savePNGDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo {
	if ( sov == NULL ) return;
#if WITH_PNG
	NSString* nsf = [sheet filename];
	char* savefile;
	//printf("savePanelDidEnd returnCode=%d nsf=\"%s\" \n", returnCode, savefile );
	if ( nsf == nil ) {
		return;
	}
	pthread_mutex_lock( &sovLock );
	savefile = sov->pngname;
	sov->pngname = strdup( [nsf fileSystemRepresentation] );
	sov->doPNG();
	free( sov->pngname );
	sov->pngname = savefile;
	pthread_mutex_unlock( &sovLock );
#endif
}
- (IBAction)startSaveDists:(id)sender {
	if ( sov == NULL ) return;
	NSSavePanel* sheet = [NSSavePanel savePanel];
	[sheet beginSheetForDirectory:nil file:nil modalForWindow:[view window] modalDelegate:self didEndSelector:@selector(saveDistsDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}
- (void)saveDistsDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo {
	if ( sov == NULL ) return;
	NSString* nsf = [sheet filename];
	char* savefile;
	//printf("savePanelDidEnd returnCode=%d nsf=\"%s\" \n", returnCode, savefile );
	if ( nsf == nil ) {
		return;
	}
	pthread_mutex_lock( &sovLock );
	savefile = sov->distfname;
	sov->distfname = strdup( [nsf fileSystemRepresentation] );
	sov->printDistricts();
	free( sov->distfname );
	sov->distfname = savefile;
	pthread_mutex_unlock( &sovLock );
}
- (IBAction)startSaveSolution:(id)sender {
	if ( sov == NULL ) return;
	NSSavePanel* sheet = [NSSavePanel savePanel];
	[sheet beginSheetForDirectory:nil file:nil modalForWindow:[view window] modalDelegate:self didEndSelector:@selector(saveDistsDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}
- (void)saveSolutionDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void  *)contextInfo {
	if ( sov == NULL ) return;
	NSString* nsf = [sheet filename];
	char* savefile;
	//printf("saveSolutionDidEnd returnCode=%d nsf=\"%s\" \n", returnCode, savefile );
	if ( nsf == nil ) {
		return;
	}
	pthread_mutex_lock( &sovLock );
	savefile = sov->dumpname;
	sov->dumpname = strdup( [nsf fileSystemRepresentation] );
	sov->printDistricts();
	free( sov->dumpname );
	sov->dumpname = savefile;
	pthread_mutex_unlock( &sovLock );
}

- (IBAction)writePNG:(id)sender {
	if ( sov == NULL ) return;
#if WITH_PNG
	pthread_mutex_lock( &sovLock );
	sov->doPNG();
	pthread_mutex_unlock( &sovLock );
#endif
}

- (IBAction)startOpenData:(id)sender {
	NSOpenPanel* sheet = [NSOpenPanel openPanel];
	[sheet setAllowsMultipleSelection:NO];
	[sheet beginSheetForDirectory:nil file:nil types:nil modalForWindow:[view window] modalDelegate:self didEndSelector:@selector(openDataDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}
- (void)openDataDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
	NSString* nsf = [sheet filename];
	char* filename;
	pthread_mutex_lock( &sovLock );
	if ( sov != NULL ) {
		delete sov;
	}
	cppSolver = new Solver();
	filename = strdup( [nsf fileSystemRepresentation] );
	if ( strstr( filename, "zcta" ) != NULL ) {
		sov->geoFact = openZCTA;
	} else if ( strstr( filename, "gbin" ) != NULL ) {
		sov->geoFact = openBin;
	} else {
		sov->geoFact = openUf1;
	}
	sov->inputname = filename;
	sov->districts = -10; // negative sign is a flag that means the value may be overriden by the file loaded
	sov->megaInit();
	[self setupDebugDistrictMenu];
	[self setupNodeColoringMenu];
	pthread_mutex_unlock( &sovLock );
}

/* NSKeyValueCoding */
- (id)valueForKey:(NSString *)key {
	printf("valueForKey: %s\n", [key dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES]);
	return nil;
}
- (id)valueForKeyPath:(NSString *)keyPath {
	printf("valueForKeyPath: %s\n", [keyPath dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES]);
	return nil;
}
- (id)valueForUndefinedKey:(NSString *)key {
	printf("valueForUndefinedKey: %s\n", [key dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES]);
	return nil;
}

- (void)setupDebugDistrictMenu {
	if ( sov->districts <= 0 ) {
		return;
	}
	[debugDistrictMenu removeAllItems];
	[debugDistrictMenu addItemWithTitle:@"All"];
	for (int i = 1; i <= sov->districts; ++i) {
		[debugDistrictMenu addItemWithTitle:[NSString stringWithFormat:@"%d",i]];
	}
	[debugDistrictMenu selectItemAtIndex:0];
}

- (void)setupNodeColoringMenu {
	[nodeColoringMenu removeAllItems];
	int i = 0;
	const char* nodeColoringName;
	while ((nodeColoringName = getNodeColoringName(i)) != NULL) {
		[nodeColoringMenu addItemWithTitle:[NSString stringWithCString:nodeColoringName]];
		++i;
	}
	[nodeColoringMenu selectItemAtIndex:0];
}

- (NSTextView*)extraTextIfAppropriate {
	if (extraText == NULL) {
		return NULL;
	}
	NSWindow* etw = [extraText window];
	if ((etw == NULL) || (![etw isVisible])) {
		return NULL;
	}
	return extraText;
}

- (void)updateStatsPane {
	char buf[512];
	if ( sov == NULL ) return;
	//fprintf(stderr,"SolverM.updateStatsPane\n");
	pthread_mutex_lock( &sovLock );
	int statStrLen = sov->getDistrictStats( buf, sizeof(buf) );
	if (statStrLen + 10 < sizeof(buf)) {
		snprintf(buf + statStrLen, sizeof(buf) - statStrLen, "fps=%lf", sov->fps );
	}
	pthread_mutex_unlock( &sovLock );
	[stats setStringValue:[NSString stringWithCString:buf]];
	[self setExtraText];
}

- (void)setExtraText {
	Solver* lsov = ((Solver*)cppSolver);
	NSTextView* et = [self extraTextIfAppropriate];
	if (et == NULL) return;
	pthread_mutex_lock( &sovLock );
	char* debugtext = lsov->dists->debugText();
	pthread_mutex_unlock( &sovLock );
	if (debugtext == NULL) return;
	NSString* nsdebugtext = [NSString stringWithCString:debugtext encoding:NSASCIIStringEncoding];
	free(debugtext);
	[et setString:nsdebugtext];
}

/* run thread */

- (void*)runThreadProc {
	int displayEvery = 50;
	int displayCountdown = displayEvery;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#if 0
	// FIXME, use actual messaging
	while ( gc == nil ) {
		usleep(10);
	}
#endif
	while ( 1 ) {
		int err;
		BOOL needsDisplay;
		pthread_mutex_lock( &(runControlM) );
		displayCountdown--;
		needsDisplay = (displayCountdown == 0) ? YES : NO;
checkMode:
			switch ( runMode ) {
				case kSolverStopped:	// stopped
					pthread_cond_wait( &runControl, &runControlM );
					goto checkMode;
				case kSolverRunning:	// running
					break;
				case kSolverStepping:	// stepping
					runMode = kSolverStopped;
					needsDisplay = YES;
					break;
				default:
					//dbmark();
					fprintf( stderr, "bogus run mode %d\n", runMode );
			}
		pthread_mutex_unlock( &(runControlM) );
		err = [self stepDisplay:needsDisplay];
		if ( err < 0 ) {
			runMode = kSolverStopped;
		}
		if ( displayCountdown == 0 ) {
			displayCountdown = displayEvery;
		}
	}
	[pool release];
}
@end

void* runThreadProc( void* arg ) {
	SolverM* it = (SolverM*) arg;
	return [it runThreadProc];
}
