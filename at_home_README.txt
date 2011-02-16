In an ideal world, all you have to do is run:

./run_redistricter

By default it will download data and start two threads of the redistricting solver and run a local web server to provide status at:
http://localhost:9988/

--port can run that on a a different port, or '--port=' should disable it.

If you want to use more or fewer cores for processing (the scripts try to run the process completely `nice`) use --threads and some number 1 or more.

touch work/stop to gracefully stop with a cycle is done
touch work/reload to make this script exec itself at that time
(these files are deleted after they're detected so that they only apply once)

Some interesting options to pass to the underlying script after '--':
-verbose

-diskQuota=1000000000
# Enabling this virtually unlimited caching locally allows download of all 50 states of data to run on.


Open Source

The source to the redistricting solver and ancillary programs is available at:
http://code.google.com/p/redistricter

Bundled into the distributed binaries may be code from:

The Google Protobuf library
http://code.google.com/p/protobuf

libpng
http://www.libpng.org/pub/png/libpng.html
