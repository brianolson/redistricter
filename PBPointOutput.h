#ifndef PB_POINT_OUTPUT_H
#define PB_POINT_OUTPUT_H

#include "tiger/rasterizeTiger.h"
#include "redata.pb.h"

class PBPointOutput : public PointOutput {
public:
	PBPointOutput(int fileDescriptor);
	PBPointOutput(int fileDescriptor, int sizex, int sizey);
	virtual ~PBPointOutput();
	virtual bool writePoint(uint64_t ubid, int px, int py);
	// nop. always true. relies on close()
	virtual bool flush();
	virtual bool close();
private:
	int fd;
	MapRasterization* rast;
	MapRasterization::Block* block;
};

#endif /* PB_POINT_OUTPUT_H */
