#include "PBPointOutput.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/gzip_stream.h>

PBPointOutput::PBPointOutput(int fileDescriptor)
	: fd(fileDescriptor), rast(NULL), block(NULL) {
	rast = new MapRasterization();
}
PBPointOutput::PBPointOutput(int fileDescriptor, int sizex, int sizey)
	: fd(fileDescriptor), rast(NULL), block(NULL) {
	rast = new MapRasterization();
	rast->set_sizex(sizex);
	rast->set_sizey(sizey);
}
PBPointOutput::~PBPointOutput() {
	delete rast;
}

bool PBPointOutput::writePoint(uint64_t ubid, int px, int py) {
	if ((block == NULL) || (block->ubid() != ubid)) {
		block = rast->add_block();
		block->set_ubid(ubid);
	}
	block->add_xy(px);
	block->add_xy(py);
	return true;
}
bool PBPointOutput::flush() {
	return true;
}
bool PBPointOutput::close() {
	google::protobuf::io::FileOutputStream pbfos(fd);
	google::protobuf::io::GzipOutputStream::Options zos_options;
	zos_options.format = google::protobuf::io::GzipOutputStream::ZLIB;
	google::protobuf::io::GzipOutputStream zos(
		&pbfos, zos_options);
	bool ok = rast->SerializeToZeroCopyStream(&zos);
	zos.Flush();
	zos.Close();
	pbfos.Close();
	return ok;
}
