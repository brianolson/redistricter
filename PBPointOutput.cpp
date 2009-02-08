#include "PBPointOutput.h"

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
	return rast->SerializeToFileDescriptor(fd);
}
