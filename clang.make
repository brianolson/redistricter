-include localvars.make

CC=${CLANGPREFIX}clang -emit-llvm -DWITH_PNG=0
CXX=${CLANGPREFIX}clang -emit-llvm -DWITH_PNG=0
LDFLAGS+=-lstdc++
CXXLD=${CLANGPREFIX}llvm-ld
LDPNG:=


clang_all:	districter2

include Makefile
