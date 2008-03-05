//
//  main.m
//  guidistricter
//
//  Created by Brian Olson on 1/30/05.
//  Copyright __MyCompanyName__ 2005. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

int main(int argc, char *argv[])
{
#ifndef NDEBUG
	struct rlimit mylimit = { 0, 0 };
	// limit self to half a gig of RAM, stop leaky runaway early.
	mylimit.rlim_max = 512*1024*1024;
	mylimit.rlim_cur = mylimit.rlim_max;
	setrlimit(RLIMIT_DATA, &mylimit);
#endif
    return NSApplicationMain(argc,  (const char **) argv);
}
