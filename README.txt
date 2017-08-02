DEPENDENCIES:
	C++ compiler (GCC or CLANG)
	Java 2.7+
	Python 3+
	png library
	zlib compression library
	google protocol buffers
	http://code.google.com/p/protobuf/ *

BUILDING:	

There's no ./configure because autoconf is hard. :-(

Instead, Makefile includes makeopts/`uname`.pre early on to set variables. See the Darwin and Linux examples.

So, if you're lucky, all you have to do is:
	make all

There is no `make install` because this is a tool for an experimenter. The binaries just live here where they're compiled.

If you want to leave makeopts/* alone, create a file localvars.make which gets included after that and before anything else. A good place to put your own settings.



RUNNING:

Setup a python environment:

# your Python version may vary, run `python3 --version` to check
pyvenv-3.6 ve
# this assumes bash or sh-like shell, to source a script into your session
. ve/bin/activate
pip install -r python/requirements.txt

Make a directory, which I traditionally call "data", to hold the Census data, and fetch a state of data into it, run preprocessing:

mkdir data
./bin/setupstatedata ct

Run from local data:

./bin/run_devmode --port=8080

The --port argument is optional, but runs a handy HTTP server on that port (e.g. http://localhost:8080/ ) which nicely displays status and results.

Lots of other options are available on both ./setupstatedata and ./run_devmode and there is --help available for them.


---
* Setting up protobuf
# Requires protobuf 2.6.1; has not been updated for protobuf 3.0
# Here's the cheat sheet for building this quickly.
# On Mac, if you want fancy multi-platform binaries, one of these (bash syntax):
CXXFLAGS='-arch ppc -arch ppc64 -arch i386 -arch x86_64' ./configure --enable-dependency-tracking=no
CXXFLAGS='-arch i386 -arch x86_64' ./configure --enable-dependency-tracking=no
# Otherwise just:
./configure

# then build C and optionally python
make
make check
sudo make install

cd ../python
python setup.py test
python setup.py install
