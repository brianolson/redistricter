This is Open Source software to create impartial optimally compact district maps for US State Legislatures and Congressional districts.

Results can be seen here:
http://bdistricting.com/2010/

And with analysis of partisan outcomes models here:
https://projects.fivethirtyeight.com/redistricting-maps/#algorithmic-compact


DEPENDENCIES:
	C++ compiler (GCC or CLANG)
	Java 1.8+
	Python 3.6+
	png library
	zlib compression library
	google protocol buffers http://code.google.com/p/protobuf/

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
python3 -m venv ve
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
# Here's the cheat sheet for building this quickly.
./configure

# then build C and optionally python
make
make check
sudo make install

# while Python virtual environment setup above is active:
cd ../python
python setup.py test
python setup.py install


DEVELOPMENT:

data.txt description of files and data bundles involved in running the system
