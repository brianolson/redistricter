DEPENDENCIES:
	C++ compiler
	Java 2.5+
	Python 2.4+
	png library
	zlib compression library
	google protocol buffers
	http://code.google.com/p/protobuf/ *

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
./setupstatedata.py ga

The 'runallstates.py' script can be used to repeatedly run one or more states.
./runallstates.py --port=8080 ga

The --port argument is optional, but runs a handy HTTP server on that port (e.g. http://localhost:8080/ ) which nicely displays status and results.

Lots of other options are available on both ./setupstatedata.py and ./runallstates.py and there is --help available for them.

---
* Setting up protobuf
# Here's the cheat sheet for building this quickly.
# On Mac, one of these (bash syntax):
CXXFLAGS='-arch ppc -arch ppc64 -arch i386 -arch x86_64' ./configure --enable-dependency-tracking=no
CXXFLAGS='-arch i386 -arch x86_64' ./configure --enable-dependency-tracking=no
# Otherwise just:
./configure

# then build, C, Java, and optionally python
make
make check
sudo make install
cd java
mvn test
mvn install
mvn package
cd ../python
python setup.py test
python setup.py install
