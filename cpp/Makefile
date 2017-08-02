SRCDIR?=${PWD}
ROOTDIR?=${PWD}/..
UNAME:=$(shell uname)

-include makeopts/${UNAME}.pre
-include localvars.make

include cpp.make

-include *.d */*.d

-include localtail.make
