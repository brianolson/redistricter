#include "GeoData.h"
#include "uf1.h"
#include "tiger/mmaped.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <zlib.h>

using std::vector;

Uf1Data::Uf1Data()
	: data(NULL), allocated(true), numIntFields(0), map(NULL) {
}
Uf1Data::~Uf1Data() {
	if ( allocated ) {
		if ( data != NULL ) {
			free(data);
		}
	} else {
		if ( map != NULL ) {
			map->close();
			delete map;
		}
	}
}

static inline char* mynextchar(char* pos, char* last, char needle) {
	while ( pos <= last ) {
		if ( *pos == needle ) {
			return pos;
		}
		pos++;
	}
	return NULL;
}
// Count comma separated elements before first newline
static int countElements(char* base, off_t lim) {
	int count = 0;
	int state = 0;
	int pos = 0;
	while ( true ) {
		char c = base[pos];
		if ( (c == '\n') || (c == '\r') ) {
			return count;
		}
		if ( c == ',' ) {
			if ( state == 0 ) {
				// empty string previous field, count it I guess
				fprintf(stderr, "warning, empty field %d\n", count + 1);
				count++;
			} else {
				state = 0;
			}
		} else if ( state == 0 ) {
			count++;
			state = 1;
		}
		pos++;
	}
}

static inline void writeOrDieF(int fd, const void* buf, size_t len, const char* file, int line) {
	ssize_t err = write(fd, buf, len);
	if ((err < 0) || (((size_t)err) != len)) {
		fprintf(stderr, "%s:%d %s\n", file, line, strerror(errno));
		exit(1);
	}
}
#define writeOrDie(a,b,c) writeOrDieF(a,b,c,__FILE__,__LINE__)

static char nullchars[7] = "\0\0\0\0\0\0";

static void countColumnMaxes(const char* filename, mmaped& mf, int32_t numbersPerLine, int32_t* maxes);

// parse a decimal integer from a line of comma separated text.
bool uint32_field_from_csv(uint32_t* out, const char* line, int column) {
	assert(line != NULL);
	// pos points to start of row
	const char* rp = line;
	int ci = 0;
	while (ci < column) {
		rp = strchr(rp, ',');
		if (rp == NULL) {
			return false;
		}
		rp++;
		ci++;
	}
	char* check;
	uint32_t t = strtoul(rp, &check, 10);
	if (check == NULL) return false;
	if (check == rp) return false;
	*out = t;
	return true;
}

class LineSource {
public:
	LineSource() {
	}
	virtual ~LineSource() {
	}
	virtual bool init(const char* filename) = 0;
	virtual char* gets(char* buffer, size_t len) = 0;
};

class FILELineSource : public LineSource {
public:
	FILELineSource() : fin(NULL) {
	}
	virtual bool init(const char* filename) {
		if ((filename == NULL) || (filename[0] == '\0')) {
			fin = stdin;
			return true;
		}
		fin = fopen(filename, "r");
		if (fin == NULL) {
			perror(filename);
			return false;
		}
		return true;
	}
	virtual ~FILELineSource() {
		if (fin != NULL) {
			fclose(fin);
			fin = NULL;
		}
	}
	virtual char* gets(char* buffer, size_t len) {
		return fgets(buffer, len, fin);
	}
private:
	FILE* fin;
};

class ZlibLineSource : public LineSource {
public:
	ZlibLineSource() : zf(NULL) {
	}
	virtual bool init(const char* filename) {
		zf = gzopen(filename, "rb");
		if (zf == NULL) {
			fprintf(stderr, "%s: %s\n", filename, gzerror(zf, NULL));
			return false;
		}
		return true;
	}
	virtual ~ZlibLineSource() {
		if (zf != NULL) {
			gzclose(zf);
			zf = NULL;
		}
	}
	virtual char* gets(char* buffer, size_t len) {
		return gzgets(zf, buffer, len);
	}
private:
	gzFile zf;
};

#define MAX_LINE_LENTH (32*1024)

// Returns malloc() buffer full of uint32_t[gd->numPoints] of uf1 data.
uint32_t* read_uf1_for_recnos(GeoData* gd, const char* filename, int column) {
	if (gd->recno_map == NULL) {
		return NULL;
	}
	LineSource* fin;
	if (strstr(filename, ".gz")) {
		fin = new ZlibLineSource();
	} else {
		fin = new FILELineSource();
	}
	if (!fin->init(filename)) {
		perror(filename);
	}
	uint32_t* out = (uint32_t*)malloc(gd->numPoints * sizeof(uint32_t));
	int line_number = 0;
	char* line = (char*)malloc(MAX_LINE_LENTH);
	while (fin->gets(line, MAX_LINE_LENTH)) {
		uint32_t recno;
		uint32_t index;
		line_number++;
		if (!uint32_field_from_csv(&recno, line, 4)) {
			fprintf(stderr, "%s:%d csv parse fail getting field 5 LOGRECNO\n", filename, line_number);
			free(out);
			out = NULL;
			goto done;
		}
		index = gd->indexOfRecno(recno);
		if (index != ((uint32_t)-1)) {
			uint32_t value;
			if (!uint32_field_from_csv(&value, line, column)) {
				fprintf(stderr, "%s:%d csv parse fail getting field %d\n", filename, line_number, column);
				free(out);
				out = NULL;
				goto done;
			}
			out[index] = value;
		}
	}
done:
	free(line);
	return out;
}


// Returns malloc() buffer full of uint32_t[gd->numPoints] of uf1 data.
bool read_uf1_columns_for_recnos(
		GeoData* gd, const char* filename,
		const vector<int>& columns,
		vector<uint32_t*>* data_columns,
		int* recnos_matched) {
	bool good = true;
	if (gd->recno_map == NULL) {
		return false;
	}
	LineSource* fin;
	if (strstr(filename, ".gz")) {
		fin = new ZlibLineSource();
	} else {
		fin = new FILELineSource();
	}
	if (!fin->init(filename)) {
		perror(filename);
	}
	data_columns->clear();
	for (unsigned int i = 0; i < columns.size(); ++i) {
		uint32_t* out = (uint32_t*)malloc(gd->numPoints * sizeof(uint32_t));
		data_columns->push_back(out);
	}
	int match_count = 0;
	int line_number = 0;
	char* line = (char*)malloc(MAX_LINE_LENTH);
	while (fin->gets(line, MAX_LINE_LENTH)) {
		uint32_t recno;
		uint32_t index;
		line_number++;
		if (!uint32_field_from_csv(&recno, line, 4)) {
			fprintf(stderr, "%s:%d csv parse fail getting field 5 LOGRECNO\n", filename, line_number);
			good = false;
			goto done;
		}
		index = gd->indexOfRecno(recno);
		if (index != ((uint32_t)-1)) {
			match_count++;
			for (unsigned int i = 0; i < columns.size(); ++i) {
				int column = columns[i];
				uint32_t value;
				if (!uint32_field_from_csv(&value, line, column)) {
					fprintf(stderr, "%s:%d csv parse fail getting field %d\n", filename, line_number, column);
					good = false;
					goto done;
				}
				uint32_t* out = (*data_columns)[i];
				out[index] = value;
			}
		}
	}
done:
	if (!good) {
		for (unsigned int i = 0; i < data_columns->size(); ++i) {
			free((*data_columns)[i]);
		}
		data_columns->clear();
	}
	free(line);
	if (recnos_matched != NULL) {
		*recnos_matched = match_count;
	}
	return good;
}

class Uf1ScanState {
public:
	Uf1ScanState(void* data, size_t size)
	: base((char*)data), pos(base), last(base + (size - 1)),
	state(0), thisLineElementIndex(0), lineCount(0) {
		nextcomma = mynextchar(base, last, ',');
		assert(nextcomma != NULL);
		nextnl = mynextchar(base, last, '\n');
		assert(nextnl != NULL);
		numbersPerLine = countElements(base, size) - 2;
		assert(numbersPerLine > 0);
		assert(numbersPerLine < 10000);
	}
	
	// parse int in 0-indexed column of comma separated data.
	uint32_t uint32_field(int column) {
		assert(pos != NULL);
		// pos points to start of row
		char* rp = pos;
		int ci = 0;
		while (ci < column) {
			rp = mynextchar(rp, last, ',');
			assert(rp != NULL);
			assert(rp < nextnl);
			rp++;
			ci++;
		}
		char* check;
		uint32_t out = strtoul(rp, &check, 10);
		assert(check != NULL);
		assert(check != rp);
		return out;
	}
	// return true if nextRow moves to a valid row. false on eof.
	bool nextRow() {
		pos = nextnl;
		if (pos == NULL) {
			return false;
		}
		while ( (pos <= last) && ((*pos == '\n') || (*pos == '\r')) ) {
			pos++;
		}
		nextnl = mynextchar(pos, last, '\n');
		if (pos == last) {
			pos = NULL;
			return false;
		}
		return true;
	}
	char* base;
	char* pos;
	char* last;
	char* nextcomma;
	char* nextnl;
	int state;
	int thisLineElementIndex;
	int lineCount;
	int32_t numbersPerLine;
};

static int processTextMmapped(const char* filename, const char* outname, mmaped& mf) {
	int toret = 0;
	int out;
	out = open(outname, O_WRONLY|O_CREAT, 0666);
	if ( out < 0 ) {
		mf.close();
		perror(outname);
		return -1;
	}
	char* base = (char*)mf.data;
	char* last = base + (mf.sb.st_size - 1);
	char* pos = base;
	char* nextcomma = mynextchar(base, last, ',');
	char* nextnl = mynextchar(base, last, '\n');
	int state = 0;
	int thisLineElementIndex = 0;
	int lineCount = 0;
	int32_t numbersPerLine;
	int32_t* values;
	int32_t* maxes;
	if ( nextnl == NULL ) {
		fprintf(stderr, "%s: no \\n found in entire file!\n", filename );
		toret = -1;
		goto finish;
	}
	if ( nextcomma == NULL ) {
		fprintf(stderr, "%s: no \',\' found in entire file!\n", filename );
		toret = -1;
		goto finish;
	}
	numbersPerLine = countElements(base, mf.sb.st_size) - 2;
	fprintf(stderr, "%s: %d elements per line\n", filename, numbersPerLine);
	assert(numbersPerLine > 0);
	assert(numbersPerLine < 10000);
	values = new int32_t[numbersPerLine];
	maxes = new int32_t[numbersPerLine];
	// first, count numbers in the first line (will later assert that all lines are equal)
	// Write out header data: filetype, state, number of elements
	{
		int tlen = nextcomma - pos;
		// write out file type tag
		writeOrDie(out, pos, tlen);
		if ( tlen < 6 ) {
			writeOrDie(out, nullchars, 6 - tlen);
		}
		pos = nextcomma + 1;
		nextcomma = mynextchar(pos, last, ',');
		assert((nextcomma - pos) == 2);
		writeOrDie(out, pos, 2);
		pos = nextcomma + 1;
		nextcomma = mynextchar(pos, last, ',');
		assert(nextcomma != NULL);
		state = 1;
		thisLineElementIndex = 0;
		writeOrDie(out, &numbersPerLine, 4);
	}
	countColumnMaxes(filename, mf, numbersPerLine, maxes);
	while ( pos <= last ) {
		switch ( state ) {
			case 0: { // start of line
				// skip first two fields: filetype,state
				nextcomma = mynextchar(pos, last, ',');
				assert(nextcomma != NULL);
				pos = nextcomma + 1;
				nextcomma = mynextchar(pos, last, ',');
				assert(nextcomma != NULL);
				pos = nextcomma + 1;
				state = 1;
				thisLineElementIndex = 0;
			}
				break;
			case 1: { // data members
				char* endptr;
				values[thisLineElementIndex] = strtol(pos, &endptr, 10);
				assert(endptr != pos);
				thisLineElementIndex++;
				if ( (*endptr == '\n') || (*endptr == '\r') ) {
					assert(thisLineElementIndex == numbersPerLine);
					writeOrDie(out, values, 4*numbersPerLine);
					lineCount++;
					state = 0;
					pos = endptr + 1;
					while ( (pos <= last) && ((*pos == '\n') || (*pos == '\r')) ) {
						pos++;
					}
				} else {
					pos = endptr + 1;
				}
			}
				break;
			default:
				assert(false);
				break;
		}
	}
	fprintf(stderr, "%s: %d lines\n", filename, lineCount);
	finish:
	mf.close();
	close(out);
	return toret;
}

void countColumnMaxes(const char* filename, mmaped& mf, int32_t numbersPerLine, int32_t* maxes) {
	char* base = (char*)mf.data;
	char* last = base + (mf.sb.st_size - 1);
	char* pos = base;
	char* nextcomma = mynextchar(base, last, ',');
//	char* nextnl = mynextchar(base, last, '\n');
	int state = 0;
	int thisLineElementIndex = 0;
	int lineCount = 0;
	for ( int i = 0; i < numbersPerLine; i++ ) {
		maxes[i] = 0;
	}
	while ( pos <= last ) {
		switch ( state ) {
			case 0: { // start of line
				// skip first two fields: filetype,state
				nextcomma = mynextchar(pos, last, ',');
				assert(nextcomma != NULL);
				pos = nextcomma + 1;
				nextcomma = mynextchar(pos, last, ',');
				assert(nextcomma != NULL);
				pos = nextcomma + 1;
				state = 1;
				thisLineElementIndex = 0;
			}
				break;
			case 1: { // data members
				char* endptr;
				int x = strtol(pos, &endptr, 10);
				if ( x > maxes[thisLineElementIndex] ) {
					maxes[thisLineElementIndex] = x;
				}
				assert(endptr != pos);
				thisLineElementIndex++;
				if ( (*endptr == '\n') || (*endptr == '\r') ) {
					assert(thisLineElementIndex == numbersPerLine);
					lineCount++;
					state = 0;
					pos = endptr + 1;
					while ( (pos <= last) && ((*pos == '\n') || (*pos == '\r')) ) {
						pos++;
					}
				} else {
					pos = endptr + 1;
				}
			}
				break;
			default:
				assert(false);
				break;
		}
	}
#if 0
	for ( int i = 0; i < numbersPerLine; i++ ) {
		fprintf(stderr,"%d,", maxes[i]);
	}
	fprintf(stderr,"\n");
#endif
}

int Uf1Data::processText(const char* filename, const char* outname) {
	mmaped mf;
	int out;
	int err = mf.open(filename);
	if ( err != 0 ) {
		int fd = open(filename, O_RDONLY, 0);
		if ( fd < 0 ) {
			perror(filename);
			return -1;
		}
		out = open(outname, O_WRONLY|O_CREAT, 0666);
		if ( out < 0 ) {
			close(fd);
			perror(outname);
			return -1;
		}
		err = processText(fd, out);
		close(fd);
		close(out);
		return err;
	}
	return processTextMmapped(filename, outname, mf);
}
// TODO: process text from arbitrary input text chunks
// TODO: process text from zlib input stream
int Uf1Data::processText(int fd, int out) {
	assert(false);
	return -1;
}

int Uf1Data::mapBinary(const char* filename) {
	map = new mmaped();
	assert(map != NULL);
	int err = map->open(filename);
	if (err != 0) {
		return -1;
	}
	allocated = false;
	data = map->data;
return -1;
}

int* getColumn(GeoData* gd, int column, int fill) {
	int numPoints = gd->numPoints;
	int* out = new int[numPoints];
	for (int i = 0; i < numPoints; ++i) {
		out[i] = fill;
	}
	return out;
}

// return a data value
int Uf1Data::value(int row, int column) {
return -1;
}
