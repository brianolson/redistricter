#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
using google::protobuf::int32;
using google::protobuf::uint32;
using google::protobuf::int64;
using google::protobuf::uint64;
using google::protobuf::io::FileInputStream;
using google::protobuf::io::GzipInputStream;
using google::protobuf::io::CodedInputStream;
#include "redata.pb.h"


int main(int argc, const char** argv) {
	char* filename = NULL;
	int fd = STDIN_FILENO;
	
	FileInputStream* fis = new FileInputStream(fd);
	GzipInputStream* gis = new GzipInputStream(fis);
	CodedInputStream* fin = new CodedInputStream(gis);
	
	StatLogEntry le;
	uint32 len;
	bool ok = fin->ReadVarint32(&len);
	while (ok) {
		CodedInputStream::Limit l = fin->PushLimit(len);
		le.Clear();
		ok = le.ParseFromCodedStream(fin);
		if (!ok) {
			fprintf(stderr, "parse failed\n");
			break;
		}
		printf("%s\n\n", le.DebugString().c_str());
		fin->PopLimit(l);
		ok = fin->ReadVarint32(&len);
	}
	return 0;
}
