DEPENDENCIES:
	google protocol buffers
	http://code.google.com/p/protobuf/

BUILDING:	

There's no ./configure because autoconf is hard. :-(

Instead, Makefile includes makeopts/`uname`.pre early on to set variables. See the Darwin and Linux examples.

So, if you're lucky, all you have to do is:
	make

There is no `make install` because this is a tool for an experimenter. The binaries just live here where they're compiled.

If you want to leave makeopts/* alone, create a file localvars.make which gets included after that and before anything else. A good place to put your own settings.

## MAC OS Notes:
makeopts/Darwin.pre
	Defaults to compiling for ppc (32 bit) and x86_64. For development it's probably better to pick just your architecture and enable the -MMD flag which emits good dependency information that gnumake can use.

RUNNING:

Make a directory, which I traditionally call "data", to hold the Census data.
./setupstatedata.pl ga
