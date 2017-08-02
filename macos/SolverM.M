#import "SolverM.h"
#import "SolverPixView.h"
#include "GeoData.h"
#include "MapDrawer.h"
#include "Solver.h"
#include "GrabIntermediateStorage.h"

#include <unistd.h>

#define sov ((Solver*)cppSolver)
#define mapr ((MapDrawer*)cppMapDrawer)

void* runThreadProc( void* );

extern "C" const char* getNodeColoringName(int index);
extern "C" void setNodeColoringByIndex(int index);
extern "C" void setDebugDistrictNumber(int index);

@interface SolverM_ParamTableAdaptor : NSObject
{
	SolverM* s;
	
	NSMutableArray* paramNames;
	NSMutableArray* paramValues;
}
// May be called again to reset internal state from new solver dists
- (id)initFromSolver: (SolverM*)s_;

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView;
- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id < NSDraggingInfo >)info row:(NSInteger)row dropOperation:(NSTableViewDropOperation)operation;
- (NSArray *)tableView:(NSTableView *)aTableView namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination forDraggedRowsWithIndexes:(NSIndexSet *)indexSet;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex;
- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex;
- (void)tableView:(NSTableView *)aTableView sortDescriptorsDidChange:(NSArray *)oldDescriptors;
- (NSDragOperation)tableView:(NSTableView *)aTableView validateDrop:(id < NSDraggingInfo >)info proposedRow:(NSInteger)row proposedDropOperation:(NSTableViewDropOperation)operation;
- (BOOL)tableView:(NSTableView *)aTableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard *)pboard;
@end

@implementation SolverM_ParamTableAdaptor
- (id)initFromSolver: (SolverM*)s_ {
	s = s_;
	int len = [s numParameters];
	paramNames = [NSMutableArray arrayWithCapacity:len];
	paramValues = [NSMutableArray arrayWithCapacity:len];
	for (int i = 0; i < len; ++i) {
		const char* str = [s getParameterLabelByIndex:i];
		[paramNames addObject:[NSString stringWithCString:str encoding:[NSString defaultCStringEncoding]]];
		double vald = [s getParameterValueByIndex:i];
#if 1
		NSNumber* val = [NSNumber numberWithDouble:vald];
#else
		NSCell* val = [[NSCell alloc] initTextCell:@""];
		[val setEditable:YES];
		[val setDoubleValue:vald];
#endif
		[paramValues addObject:val];
		fprintf(stderr, "%d: %s=%f\n", i, str, vald);
	}
	return self;
}
- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView {
	int rows = [s numParameters];
	fprintf(stderr, "numberOfRowsInTableView:%p => %d\n", aTableView, rows);
	return rows;
}
- (BOOL)tableView:(NSTableView *)aTableView acceptDrop:(id < NSDraggingInfo >)info row:(NSInteger)row dropOperation:(NSTableViewDropOperation)operation {
	// drag-in unimplemented
	fprintf(stderr, "acceptDrop\n");
	return NO;
}
- (NSArray *)tableView:(NSTableView *)aTableView namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination forDraggedRowsWithIndexes:(NSIndexSet *)indexSet {
	fprintf(stderr, "namesOfPromisedFilesDroppedAtDestination\n");
	return NULL;
}
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {
	// Return Cell object for column:row
	// TODO: setIdentifier to be paramNames and paramValues
	NSMutableArray* ar = nil;
	id tcid = [aTableColumn identifier];
	if ([tcid isKindOfClass:[NSString class]]) {
		if ([tcid isEqualToString:@"2"]) {
			//fprintf(stderr, "goint to return paramValues[%d]\n", (int)rowIndex);
			ar = paramValues;
		} else if ([tcid isEqualToString:@"1"]) {
			//fprintf(stderr, "goint to return paramNames[%d]\n", (int)rowIndex);
			ar = paramNames;
		}
	}
	if (ar != nil) {
		id tid = [ar objectAtIndex:rowIndex];
		//fprintf(stderr, "tid=%p desc=%s\n", tid, [[tid description] cStringUsingEncoding:NSASCIIStringEncoding]);
		return tid;
	}
	fprintf(stderr, "tableView:%s objectValueForTableColumn:%s rowIndex:%d\n\tidentifier %p, desc=%s\n",
			[[aTableView description] cStringUsingEncoding:NSASCIIStringEncoding],
			[[aTableColumn description] cStringUsingEncoding:NSASCIIStringEncoding],
			(int)rowIndex,
			[aTableColumn identifier],
			[[[aTableColumn identifier] description] cStringUsingEncoding:NSASCIIStringEncoding]);
	return nil;
}
- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex {
	// Accept changed data
	// TODO
	NSMutableArray* ar = nil;
	id tcid = [aTableColumn identifier];
	if ([tcid isKindOfClass:[NSString class]]) {
		if ([tcid isEqualToString:@"2"]) {
			fprintf(stderr, "going to set paramValues[%d]\n", (int)rowIndex);
			ar = paramValues;
		} else if ([tcid isEqualToString:@"1"]) {
			fprintf(stderr, "error: trying to set paramNames[%d]!\n", (int)rowIndex);
			//ar = paramNames;
		}
	}
	if (ar != nil) {
		if ([anObject respondsToSelector:@selector(doubleValue)]) {
			double x = [anObject doubleValue];
			[s setParameterByIndex:rowIndex value:x];
			NSNumber* nn = [NSNumber numberWithDouble:x];
			fprintf(stderr, "setting [%d]=%f\n", (int)rowIndex, x);
			[ar replaceObjectAtIndex:rowIndex withObject:nn];
		}
	}
	fprintf(stderr, "table=%p setObjectValue:id=%p desc=%s col=%p desc=%s row=%d\n",
			aTableView,
			anObject,
			[[anObject description] cStringUsingEncoding:NSASCIIStringEncoding],
			aTableColumn,
			[[[aTableColumn identifier] description] cStringUsingEncoding:NSASCIIStringEncoding],
			(int)rowIndex);
}
- (void)tableView:(NSTableView *)aTableView sortDescriptorsDidChange:(NSArray *)oldDescriptors {
	// Don't care
	fprintf(stderr, "sortDescriptorsDidChange\n");
}
- (NSDragOperation)tableView:(NSTableView *)aTableView validateDrop:(id < NSDraggingInfo >)info proposedRow:(NSInteger)row proposedDropOperation:(NSTableViewDropOperation)operation {
	// drag-in unimplemented
	fprintf(stderr, "validateDrop\n");
	return nil;
}
- (BOOL)tableView:(NSTableView *)aTableView writeRowsWithIndexes:(NSIndexSet *)rowIndexes toPasteboard:(NSPasteboard *)pboard {
	// drag-out unimplemented
	fprintf(stderr, "writeRowsWithIndexes\n");
	return NO;
}
@end

@implementation SolverM
- (void)awakeFromNib {
	dataPath = NULL;
	cppSolver = NULL;
	cppMapDrawer = NULL;
	isPixView = YES;
	debugParamsDataSource = nil;

	runMode = kSolverStopped;
	pthread_mutex_init( &runControlM, NULL );
	pthread_cond_init( &runControl, NULL );
	pthread_mutex_init( &sovLock, NULL );
	pthread_create( &runThread, NULL, runThreadProc, self );
#if 0
	if ( _edgeRelativeDistanceFactor ) {
		[_edgeRelativeDistanceFactor setDoubleValue: District2::edgeRelativeDistanceFactor];
	}
	if ( _odEdgeRelativeDistanceFactor ) {
		[_odEdgeRelativeDistanceFactor setDoubleValue: District2::odEdgeRelativeDistanceFactor];
	}
	if ( _popRatioFactor ) {
		[_popRatioFactor setDoubleValue: District2::popRatioFactor];
	}
#endif
#if 0
	if ( zoomBox && sov ) {
		[zoomBox setDoubleValue: sov->zoom];
	}
	if ( dcxBox && sov ) {
		[dcxBox setDoubleValue: sov->dcx];
	}
	if ( dcyBox && sov ) {
		[dcyBox setDoubleValue: sov->dcy];
	}
#endif

	if ( extraText ) {
		[extraText setFont:[NSFont fontWithName:@"Monaco" size:10]];
	}
	[self setupNodeColoringMenu];
	[solverTypeMenu removeAllItems];
	int i = 0;
	const char* setFactoryName;
	while ((setFactoryName = Solver::getSetFactoryName(i)) != NULL) {
		[solverTypeMenu addItemWithTitle:[NSString stringWithCString:setFactoryName encoding:[NSString defaultCStringEncoding]]];
		++i;
	}
	[solverTypeMenu selectItemAtIndex:0];
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
	pthread_mutex_lock( &runControlM );
	runMode = kSolverStepping;
	pthread_cond_signal( &runControl );
	pthread_mutex_unlock( &runControlM );
}

- (int)stepDisplay:(BOOL)needsDisplay {
	if ( sov == NULL ) {
		[self setupSolver];
		if ( sov == NULL ) {
			needsDisplay = NO;
			return -1;
		}
	}
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

- (void)drawPix:(NSRect) rect {
	if ( sov == NULL ) return;
	//fprintf(stderr,"SolverM.drawGL\n");
	pthread_mutex_lock( &sovLock );
	sov->recordDrawTime();
	mapr->paintPixels( sov );
	NSDrawBitmap(rect, mapr->width, mapr->height, 8, 4, 32, mapr->width * 4,
		NO, YES, NSDeviceRGBColorSpace, &(mapr->data) );
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
	if ( isPixView ) {
		NSSize newSize;
		newSize.width = mapr->width * sov->zoom;
		newSize.height = mapr->height * sov->zoom;
		[view setFrameSize:newSize];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)zoomOut:(id)sender {
	if ( sov == NULL ) return;
	sov->zoomOut();
	if ( zoomBox ) {
		[zoomBox setDoubleValue: sov->zoom];
	}
	if ( isPixView ) {
		NSSize newSize;
		newSize.width = mapr->width * sov->zoom;
		newSize.height = mapr->height * sov->zoom;
		[view setFrameSize:newSize];
	}
	[view setNeedsDisplay:YES];
}
- (IBAction)zoomAll:(id)sender {
	if ( sov == NULL ) return;
	if ( isPixView ) {
		NSSize newSize;
		newSize.width = mapr->width;
		newSize.height = mapr->height;
		NSScrollView* sv = [view enclosingScrollView];
		if ( sv != nil ) {
			NSSize svsize = [sv contentSize];
			double widthratio = svsize.width / mapr->width;
			double heightratio = svsize.height / mapr->height;
			if ( widthratio > heightratio ) {
				sov->zoom = heightratio;
			} else {
				sov->zoom = widthratio;
			}
			newSize.width *= sov->zoom;
			newSize.height *= sov->zoom;
		}
		[view setFrameSize:newSize];
	} else {
		sov->zoomAll();
	}
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
#if 0
	if ( [sender respondsToSelector:@selector(doubleValue)] ) {
		District2::edgeRelativeDistanceFactor = [sender doubleValue];
		if ( _edgeRelativeDistanceFactor ) {
			[_edgeRelativeDistanceFactor setDoubleValue: District2::edgeRelativeDistanceFactor];
		}
	} else {
		printf("setEdgeRelativeDistanceFactor from unkown sender %p\n", sender );
	}
#endif
}
- (IBAction)setOdEdgeRelativeDistanceFactor:(id)sender {
#if 0
	if ( [sender respondsToSelector:@selector(doubleValue)] ) {
		District2::odEdgeRelativeDistanceFactor = [sender doubleValue];
		if ( _odEdgeRelativeDistanceFactor ) {
			[_odEdgeRelativeDistanceFactor setDoubleValue: District2::odEdgeRelativeDistanceFactor];
		}
	} else {
		printf("setOdEdgeRelativeDistanceFactor from unkown sender %p\n", sender );
	}
#endif
}
- (IBAction)setPopRatioFactor:(id)sender {
#if 0
	if ( [sender respondsToSelector:@selector(doubleValue)] ) {
		District2::popRatioFactor = [sender doubleValue];
		if ( _popRatioFactor ) {
			[_popRatioFactor setDoubleValue: District2::popRatioFactor];
		}
	} else {
		printf("setPopRatioFactor from unkown sender %p\n", sender );
	}
#endif
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
	if ( isPixView ) {
		NSSize newSize;
		newSize.width = mapr->width * sov->zoom;
		newSize.height = mapr->height * sov->zoom;
		[view setFrameSize:newSize];
	}
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
	if ( sov == NULL ) return;
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
	NSArray* fileTypes = [NSArray arrayWithObjects:@"pb", @"gbin", /*@"zcta",*/ @"uf1", @"args", nil];
	[sheet beginSheetForDirectory:nil file:nil types:fileTypes modalForWindow:[view window] modalDelegate:self didEndSelector:@selector(openDataDidEnd:returnCode:contextInfo:) contextInfo:NULL];
}
- (void)openDataDidEnd:(NSOpenPanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
	if (returnCode != NSOKButton) {
		return;
	}
	NSString* nsf = [sheet filename];
	pthread_mutex_lock( &sovLock );
	if ( sov != NULL ) {
		delete sov;
		cppSolver = NULL;
	}
	if ( mapr != NULL ) {
		delete mapr;
		cppMapDrawer = NULL;
	}
	if ( dataPath != NULL ) {
		free( dataPath );
		dataPath = NULL;
	}
	dataPath = strdup( [nsf fileSystemRepresentation] );
	pthread_mutex_unlock( &sovLock );
}

- (void)adjustPixToRasterization {
	if ( isPixView ) {
		NSSize newSize;
		newSize.width = mapr->width;
		newSize.height = mapr->height;
		NSScrollView* sv = [view enclosingScrollView];
		if ( sv != nil ) {
			NSSize svsize = [sv contentSize];
			double widthratio = svsize.width / mapr->width;
			double heightratio = svsize.height / mapr->height;
			if ( widthratio > heightratio ) {
				sov->zoom = heightratio;
			} else {
				sov->zoom = widthratio;
			}
			newSize.width *= sov->zoom;
			newSize.height *= sov->zoom;
		}
		[view setFrameSize:newSize];
	}
}

- (void)setupSolverFinalize {
	if (debugParamsDataSource == nil) {
		debugParamsDataSource = [[SolverM_ParamTableAdaptor alloc] initFromSolver:self];
		[debugParams setDataSource:debugParamsDataSource];
	}
	[debugParams reloadData];
	mapr->initDataAndRows();
	[self setupDebugDistrictMenu];
}

// Reads file with arguments split by any whitespace according to isspace().
// \isspace() is excepted and preserved.
// "mpout:", "path/to/ST.mpout" is special and loded here.
- (void)setupSolverFromArgFile:(const char*)filename {
	pthread_mutex_lock( &sovLock );
	if ( sov != NULL ) {
		delete sov;
		cppSolver = NULL;
	}
	if ( mapr != NULL ) {
		delete mapr;
		cppMapDrawer = NULL;
	}
	cppSolver = new Solver();
	assert(sov != NULL);
	cppMapDrawer/* mapr */ = new MapDrawer();
	assert(mapr != NULL);
	// This might be overridden by the args file.
	sov->setFactoryByIndex([solverTypeMenu indexOfSelectedItem]);

	int argc;
	char** argv;
	argc = parseArgvFromFile(filename, &argv);
	if ( argc > 0 ) {
		char* mpoutName = NULL;
		char* mppbName = NULL;
		for ( int i = 1; i < argc; ++i ) {
			// find any mpout arg, load, and strip the args.
			if ( ! strcmp(argv[i], "mpout:") ) {
				mpoutName = argv[i+1];
				int j = i + 2;
				while ( j < argc ) {
					argv[i] = argv[j];
					i++;
					j++;
				}
				argv[i] = NULL;
				argc -= 2;
				break;
			} else if ( ! strcmp(argv[i], "mppb:") ) {
				mppbName = argv[i+1];
				int j = i + 2;
				while ( j < argc ) {
					argv[i] = argv[j];
					i++;
					j++;
				}
				argv[i] = NULL;
				argc -= 2;
				break;
			}
		}
		sov->handleArgs(argc, argv);
		sov->megaInit();
		if (mpoutName != NULL) {
			mapr->readUPix( sov, mpoutName );
			[self adjustPixToRasterization];
		} else if (mppbName != NULL) {
			mapr->readMapRasterization( sov, mppbName );
			[self adjustPixToRasterization];
		}
		free(argv);
	}

	[self setupSolverFinalize];
	pthread_mutex_unlock( &sovLock );
}

- (void)setupSolver {
	if ( dataPath == NULL ) {
		fprintf(stderr, "cannot setup solver, no source data path specified\n");
		return;
	}
	if ( strstr( dataPath, "args" ) != NULL ) {
		[self setupSolverFromArgFile: dataPath];
		return;
	}
	pthread_mutex_lock( &sovLock );
	if ( sov != NULL ) {
		delete sov;
		cppSolver = NULL;
	}
	if ( mapr != NULL ) {
		delete mapr;
		cppMapDrawer = NULL;
	}
	cppSolver = new Solver();
	assert(sov != NULL);
	cppMapDrawer/* mapr */ = new MapDrawer();
	assert(mapr != NULL);
	//fprintf(stderr,"loading %s\n", dataPath);
	char* suffixpos = NULL;
#if 0
	if ( strstr( dataPath, "zcta" ) != NULL ) {
		sov->geoFact = openZCTA;
	} else
#endif
	if ( (suffixpos = strstr( dataPath, "pb" )) != NULL ) {
		sov->geoFact = protobufGeoDataTag;
	} else {
		sov->geoFact = openUf1;
	}
	sov->setFactoryByIndex([solverTypeMenu indexOfSelectedItem]);
	sov->inputname = dataPath;
	sov->districts = -10; // negative sign is a flag that means the value may be overriden by the file loaded
	sov->megaInit();
	if ( suffixpos != NULL ) {
		char* mpoutName = (char*)malloc( strlen(dataPath) + 6 );
		suffixpos--;
		char* strsrc = dataPath;
		char* strdst = mpoutName;
		while (strsrc != suffixpos) {
			*strdst = *strsrc;
			strdst++; strsrc++;
		}
		strcpy( strdst, ".mpout" );
		if (access(mpoutName, R_OK) == 0) {
			//fprintf(stderr,"loading %s\n", mpoutName);
			mapr->readUPix( sov, mpoutName );
			[self adjustPixToRasterization];
		} else {
			strsrc = dataPath;
			strdst = mpoutName;
			while (strsrc != suffixpos) {
				*strdst = *strsrc;
				strdst++; strsrc++;
			}
			strcpy( strdst, ".mppb" );
			if (access(mpoutName, R_OK) == 0) {
				//fprintf(stderr,"loading %s\n", mpoutName);
				mapr->readMapRasterization( sov, mpoutName );
				[self adjustPixToRasterization];
			} else {
				fprintf(stderr, "no loadable map!\n");
				mapr->setSize( sov->pngWidth, sov->pngHeight );
			}
		}
		free( mpoutName );
	} else {
		mapr->setSize( sov->pngWidth, sov->pngHeight );
	}
	[self setupSolverFinalize];
	pthread_mutex_unlock( &sovLock );
}

/* NSKeyValueCoding */
- (id)valueForKey:(NSString *)key {
	printf("valueForKey: %s\n",
		(const char*)[[key dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES] bytes]);
	return nil;
}
- (id)valueForKeyPath:(NSString *)keyPath {
	printf("valueForKeyPath: %s\n",
		(const char*)[[keyPath dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES] bytes]);
	return nil;
}
- (id)valueForUndefinedKey:(NSString *)key {
	printf("valueForUndefinedKey: %s\n",
		(const char*)[[key dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES] bytes]);
	return nil;
}

- (void)setupDebugDistrictMenu {
	assert(sov != NULL);
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
		[nodeColoringMenu addItemWithTitle:[NSString stringWithCString:nodeColoringName encoding:[NSString defaultCStringEncoding]]];
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
	if ((statStrLen >= 0) && (((size_t)statStrLen + 10) < sizeof(buf))) {
		snprintf(buf + statStrLen, sizeof(buf) - statStrLen, "fps=%lf", sov->fps );
	}
	pthread_mutex_unlock( &sovLock );
	[stats setStringValue:[NSString stringWithCString:buf encoding:[NSString defaultCStringEncoding]]];
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

/* Pass through do Solver's DistrictSet object */
- (int)numParameters {
	if (sov == NULL) {
		return -1;
	}
	int p;
	//pthread_mutex_lock( &sovLock );
	p = sov->dists->numParameters();
	//pthread_mutex_unlock( &sovLock );
	return p;
}
- (const char*)getParameterLabelByIndex:(int)index {
	if (sov == NULL) {
		return NULL;
	}
	const char* p;
	//pthread_mutex_lock( &sovLock );
	p = sov->dists->getParameterLabelByIndex(index);
	//pthread_mutex_unlock( &sovLock );
	return p;
}
- (double)getParameterValueByIndex:(int)index {
	if (sov == NULL) {
		return NAN;
	}
	double v;
	v = sov->dists->getParameterByIndex(index);
	return v;
}
- (void)setParameterByIndex:(int)index value:(double)v {
	if (sov == NULL) {
		return;
	}
	pthread_mutex_lock( &sovLock );
	sov->dists->setParameterByIndex(index, v);
	pthread_mutex_unlock( &sovLock );
}

/* run thread */

- (void*)runThreadProc {
	int displayEvery = 50;
	int displayCountdown = displayEvery;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
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
