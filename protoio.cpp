#if HAVE_PROTOBUF

#include <google/protobuf/io/zero_copy_stream_impl.h>
//namespace google::protobuf::io

#include "districter.h"
//#include "GeoData.h"
//#include "redata.pb.h"
#include "Solver.h"

#include <fcntl.h>

// Yes, I'm including the C++ source file here, that way I get conditional
// compilation on the generated file under the "#if HAVE_PROTOBUF" that wraps
// this whole file. It's kinda gross, but I'm still not bothering to learn
// autoconf yet.
#include "redata.pb.cc"

using google::protobuf::int32;

class ProtobufGeoData : public GeoData {
	virtual int load() {
		// never call this. stuff values in with readFromProtoFile() below
		assert(false);
		return 0;
	}
};

int writeToProtoFile(Solver* sov, const char* filename) {
	int fd = open(filename, O_WRONLY|O_CREAT, 0666);
	if (fd < 0) {
		perror(filename);
		return -1;
	}
	GeoData* gd = sov->gd;
	RedistricterData rd;
	rd.Clear();
#if READ_INT_POS
	google::protobuf::RepeatedField<int32>* pos = rd.mutable_intpoints();
	pos->Reserve(gd->numPoints * 2);
	for (int i = 0; i < (gd->numPoints * 2); ++i) {
		pos->Add(gd->pos[i]);
	}
#elif READ_DOUBLE_POS
	google::protobuf::RepeatedField<double>* pos = rd.mutable_intpoints();
	pos.Reserve(gd->numPoints * 2);
	for (int i = 0; i < (gd->numPoints * 2); ++i) {
		pos.Add(gd->pos[i]);
	}
#else
#error "neither int nor double pos"
#endif
#if READ_INT_POP
	google::protobuf::RepeatedField<int32>* pop = rd.mutable_population();
	pop->Reserve(gd->numPoints);
	for (int i = 0; i < gd->numPoints; ++i) {
		pop->Add(gd->pop[i]);
	}
#endif
#if READ_INT_AREA
	google::protobuf::RepeatedField<int32>* area = rd.mutable_area();
	area->Reserve(gd->numPoints);
	for (int i = 0; i < gd->numPoints; ++i) {
		area->Add(gd->area[i]);
	}
#endif
#if READ_UBIDS
	google::protobuf::RepeatedField<google::protobuf::int64>* ubids = rd.mutable_ubids();
	ubids->Reserve(gd->numPoints);
	for (int i = 0; i < gd->numPoints; ++i) {
		ubids->Add(0);
	}
	for (int i = 0; i < gd->numPoints; ++i) {
		ubids->Set(gd->ubids[i].index, gd->ubids[i].ubid);
	}
#endif
	
	bool ok = rd.SerializeToFileDescriptor(fd);
	int err = close(fd);
	if (ok && (err == 0)) {
		return 0;
	}
	return -1;
}

int readFromProtoFile(Solver* sov, const char* filename) {
	int fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror(filename);
		return -1;
	}
	google::protobuf::io::FileInputStream pbfis(fd);
	pbfis.SetCloseOnDelete(true);
	RedistricterData rd;
	bool ok = rd.ParseFromZeroCopyStream(&pbfis);
	if (!ok) {
		return -1;
	}
	if (sov->gd != NULL) {
		delete sov->gd;
	}
	GeoData* gd = sov->gd = new ProtobufGeoData();
	if (rd.intpoints_size() > 0) {
		gd->numPoints = rd.intpoints_size() / 2;
		gd->allocPoints();
		int pi = 0;
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->set_lon(i, rd.intpoints(pi));
			++pi;
			gd->set_lat(i, rd.intpoints(pi));
			++pi;
		}
	} else if (rd.fpoints_size() > 0) {
		gd->numPoints = rd.fpoints_size() / 2;
		gd->allocPoints();
		int pi = 0;
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->set_lon(i, rd.fpoints(pi));
			++pi;
			gd->set_lat(i, rd.fpoints(pi));
			++pi;
		}
	}
#if READ_INT_POP
	if (rd.population_size() > 0) {
		gd->pop = new int32_t[gd->numPoints];
		gd->totalpop = 0;
		gd->maxpop = 0;
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->pop[i] = rd.population(i);
			if (gd->pop[i] > gd->maxpop) {
				gd->maxpop = gd->pop[i];
			}
			gd->totalpop += gd->pop[i];
		}
	}
#endif
#if READ_INT_AREA
	if (rd.area_size() > 0) {
		assert(rd.area_size() == gd->numPoints);
		gd->area = new uint32_t[gd->numPoints];
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->area[i] = rd.area(i);
		}
	}
#endif
#if READ_UBIDS
	if (rd.ubids_size() > 0) {
		assert(rd.ubids_size() == gd->numPoints);
		gd->ubids = new GeoData::UST[gd->numPoints];
		assert(gd->ubids != NULL);
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->ubids[i].ubid = rd.ubids(i);
			gd->ubids[i].index = i;
		}
	}
	extern int ubidSortF( const void* a, const void* b );
	qsort( gd->ubids, gd->numPoints, sizeof( GeoData::UST ), ubidSortF );
#endif
	return 0;
}

#endif
