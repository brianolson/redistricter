#if HAVE_PROTOBUF

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/gzip_stream.h>
using google::protobuf::int32;
using google::protobuf::int64;
using google::protobuf::uint64;

#include "BinaryStatLogger.h"
#include "LastNMinMax_inl.h"
#include "redata.pb.h"
#include "Solver.h"
#include "GeoData.h"

#include <fcntl.h>

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
	google::protobuf::RepeatedField<int32>* pop = rd.mutable_population();
	pop->Reserve(gd->numPoints);
	for (int i = 0; i < gd->numPoints; ++i) {
		pop->Add(gd->pop[i]);
	}
	google::protobuf::RepeatedField<uint64>* area = rd.mutable_area();
	area->Reserve(gd->numPoints);
	for (int i = 0; i < gd->numPoints; ++i) {
		area->Add(gd->area[i]);
	}
#if READ_UBIDS
	google::protobuf::RepeatedField<uint64>* ubids = rd.mutable_ubids();
	ubids->Reserve(gd->numPoints);
	for (int i = 0; i < gd->numPoints; ++i) {
		ubids->Add(0);
	}
	for (int i = 0; i < gd->numPoints; ++i) {
		ubids->Set(gd->ubids[i].index, gd->ubids[i].ubid);
	}
#endif
	if (gd->recnos != NULL) {
		google::protobuf::RepeatedField<int32>* recnos = rd.mutable_recnos();
		for (int i = 0; i < gd->numPoints; ++i) {
			recnos->Add(gd->recnos[i]);
		}
	}
	google::protobuf::RepeatedField<int32>* edges = rd.mutable_edges();
	for (unsigned int i = 0; i < sov->numEdges; ++i) {
		edges->Add(sov->edgeData[i*2]);
		edges->Add(sov->edgeData[i*2 + 1]);
	}
	
	bool ok = true;
	{
		google::protobuf::io::FileOutputStream pbfos(fd);
		google::protobuf::io::GzipOutputStream::Options zos_opts;
		zos_opts.format = google::protobuf::io::GzipOutputStream::ZLIB;
		google::protobuf::io::GzipOutputStream zos(&pbfos, zos_opts);
		ok = rd.SerializeToZeroCopyStream(&zos);
		zos.Flush();
		zos.Close();
		pbfos.Close();
	}
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
	google::protobuf::io::GzipInputStream zis(&pbfis);
	pbfis.SetCloseOnDelete(true);
	RedistricterData rd;
	bool ok = rd.ParseFromZeroCopyStream(&zis);
	if (!ok) {
		return -1;
	}
	if (sov->gd != NULL) {
		delete sov->gd;
	}
	GeoData* gd = sov->gd = new ProtobufGeoData();
	if (rd.intpoints_size() > 0) {
		gd->numPoints = rd.intpoints_size() / 2;
		assert(gd->numPoints > 0);
		gd->allocPoints();
		int pi = 0;
		int i = 0;
		gd->set_lon(i, rd.intpoints(pi));
		++pi;
		gd->set_lat(i, rd.intpoints(pi));
		++pi;
		gd->minx = gd->maxx = gd->lon(i);
		gd->miny = gd->maxy = gd->lat(i);
		++i;
		for (; i < gd->numPoints; ++i) {
			gd->set_lon(i, rd.intpoints(pi));
			++pi;
			gd->set_lat(i, rd.intpoints(pi));
			++pi;
		}
	} else if (rd.fpoints_size() > 0) {
		gd->numPoints = rd.fpoints_size() / 2;
		assert(gd->numPoints > 0);
		gd->allocPoints();
		int pi = 0;
		int i = 0;
		gd->set_lon(i, rd.fpoints(pi));
		++pi;
		gd->set_lat(i, rd.fpoints(pi));
		++pi;
		gd->minx = gd->maxx = gd->lon(i);
		gd->miny = gd->maxy = gd->lat(i);
		++i;
		for (; i < gd->numPoints; ++i) {
			gd->set_lon(i, rd.fpoints(pi));
			++pi;
			gd->set_lat(i, rd.fpoints(pi));
			++pi;
		}
	}
	if (rd.population_size() > 0) {
		assert(rd.population_size() == gd->numPoints);
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
	if (rd.area_size() > 0) {
		assert(rd.area_size() == gd->numPoints);
		gd->area = new uint64_t[gd->numPoints];
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->area[i] = rd.area(i);
		}
	}
#if READ_UBIDS
	fprintf(stderr, "reading ubids\n");
	if (rd.ubids_size() > 0) {
		assert(rd.ubids_size() == gd->numPoints);
		gd->ubids = new GeoData::UST[gd->numPoints];
		assert(gd->ubids != NULL);
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->ubids[i].ubid = rd.ubids(i);
			gd->ubids[i].index = i;
		}
		extern int ubidSortF( const void* a, const void* b );
		qsort( gd->ubids, gd->numPoints, sizeof( GeoData::UST ), ubidSortF );
		for (int i = 0; i < gd->numPoints; ++i) {
			assert(gd->ubids[i].index == gd->indexOfUbid(gd->ubids[i].ubid));
			int ti = gd->ubids[i].index;
			int ii = gd->indexOfUbid(gd->ubids[i].ubid);
			if (ii != ti) fprintf(stderr, "%d -> %d\n", ti, ii);
		}
	}
#endif
	if (rd.recnos_size() > 0) {
		extern int recnoSortF( const void* a, const void* b );
		assert(rd.recnos_size() == gd->numPoints);
		gd->recno_map = new Uf1::RecnoNode[gd->numPoints];
		gd->recnos = new uint32_t[gd->numPoints];
		for (int i = 0; i < gd->numPoints; ++i) {
			gd->recnos[i] = rd.recnos(i);
			gd->recno_map[i].recno = gd->recnos[i];
			gd->recno_map[i].index = i;
		}
		qsort( gd->recno_map, gd->numPoints, sizeof(Uf1::RecnoNode), recnoSortF );
	}
	sov->numEdges = rd.edges_size() / 2;
	sov->edgeData = new int32_t[rd.edges_size()];
	for (int i = 0; i < rd.edges_size(); ++i) {
		sov->edgeData[i] = rd.edges(i);
	}
	fprintf(stderr, "done reading pb\n");
	return 0;
}

class PBStatLogger : public BinaryStatLogger {
public:
	// return true if ok.
	virtual bool log(const SolverStats* it,
		LastNMinMax<double>* recentKmpp, LastNMinMax<double>* recentSpread);

	virtual ~PBStatLogger();

protected:
	PBStatLogger();
	virtual bool init(const char* name);
	
	// result of strdup. free on ~
	char* filename;
	int fd;
	google::protobuf::io::FileOutputStream* pbfos;
	google::protobuf::io::GzipOutputStream* zos;
	google::protobuf::io::CodedOutputStream* out;
	
	friend class BinaryStatLogger;
};

PBStatLogger::PBStatLogger()
	: filename(NULL), fd(-1), pbfos(NULL), zos(NULL) {}

PBStatLogger::~PBStatLogger() {
	if (out != NULL) {
		delete out;
	}
	if (zos != NULL) {
		delete zos;
	}
	if (pbfos != NULL) {
		delete pbfos;
	}
	if (fd >= 0) {
		close(fd);
	}
	if (filename != NULL) {
		free(filename);
	}
}

bool PBStatLogger::init(const char* name) {
	filename = strdup(name);
	google::protobuf::io::GzipOutputStream::Options zos_opts;
	zos_opts.format = google::protobuf::io::GzipOutputStream::ZLIB;
	
	fd = ::open(filename, O_CREAT|O_WRONLY);
	if (fd < 0) {
		perror(filename);
		return false;
	}
	pbfos = new google::protobuf::io::FileOutputStream(fd);
	if (pbfos == NULL) {
		return false;
	}
	zos = new google::protobuf::io::GzipOutputStream(pbfos, zos_opts);
	if (zos == NULL) {
		return false;
	}
	out = new google::protobuf::io::CodedOutputStream(zos);
	return out != NULL;
}

// static
BinaryStatLogger* BinaryStatLogger::open(const char* name) {
	BinaryStatLogger* it = new PBStatLogger();
	bool ok = it->init(name);
	if (!ok) {
		delete it;
		return NULL;
	}
	return it;
}

bool PBStatLogger::log(
		const SolverStats* it,
		LastNMinMax<double>* recentKmpp, LastNMinMax<double>* recentSpread) {
	StatLogEntry le;
	le.set_generation(it->generation);
	le.set_kmpp(it->avgPopDistToCenterOfDistKm);
	le.set_averagepopulation(it->popavg);
	le.set_popstddev(it->popstd);
	le.set_maxpop(it->popmax);
	le.set_minpop(it->popmin);
	le.set_medianpop(it->popmed);
	if (it->nod > 0) {
		le.set_nodistrictblocks(it->nod);
		le.set_nodistrictpop(it->nodpop);
	}
	if (recentKmpp != NULL) {
		le.set_kmppvariability(
			(1.0 * recentKmpp->max() - recentKmpp->min()) / recentKmpp->last());
	}
	if (recentSpread != NULL) {
		le.set_spreadvariability(
			(1.0 * recentSpread->max() - recentSpread->min()) / it->popavg);
	}
	int lesize = le.ByteSize();
	out->WriteVarint32(lesize);
	return le.SerializeToCodedStream(out);
}

#else
// static
BinaryStatLogger* BinaryStatLogger::open(const char* name) {
	return NULL;
}
#endif
