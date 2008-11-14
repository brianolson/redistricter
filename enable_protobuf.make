# copy this into localvars.make
CCOMMONFLAGS:=-m64 -I/usr/X11/include -L/usr/X11/lib -MMD -DHAVE_PROTOBUF
LDFLAGS:=-lprotobuf
# protoc redata.proto --cpp_out=.
protoio.o:	redata.pb.cc
%.pb.cc : %.proto
	protoc $< --cpp_out=$(@D)
