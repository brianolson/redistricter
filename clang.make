CLANGPREFIX?=/Users/bolson/psrc/llvm/llvm/Debug+Asserts/bin/
CC=${CLANGPREFIX}clang -emit-llvm
CXX=${CLANGPREFIX}clang -emit-llvm
LDFLAGS+=-L/usr/lib/gcc/i686-apple-darwin10/4.2.1
LDFLAGS+=-lstdc++
CXXLD=${CLANGPREFIX}llvm-ld


clang_all:	districter2

include Makefile
