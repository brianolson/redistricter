/*
 * place map: file of bigendian (uint64,uint64) mapping ubid to place number.
 * place names: '|' separated text lines, e.g.
 * OH|39|00464|Adena village|Incorporated Place|A|Harrison County, Jefferson County^M
 * {state}|{state fips number}|{place number}|{place name}|{place type text}|{place type code}|{place counties, comma separated}\r\n
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>

#include <algorithm>
#include <memory>

#include "placefile.h"

// They do this in the text file where a place is a 5 digit decimal number
uint64_t PlaceMap::INVALID_PLACE = 99999;

uint64_t PlaceMap::placeForUbid(uint64_t ubid) {
    // binary search across ubid, return place number
    int hi = numrecords - 1;
    int lo = 0;
    int mid = (hi + lo) / 2;
    while (lo <= hi) {
        uint64_t mv = data[mid*2 + 0];
        if (mv == ubid) {
            return data[mid*2 + 1];
        } else if (mv < ubid) {
            lo = mid + 1;
            mid = (hi + lo) / 2;
        } else /* mv > ubid */ {
            hi = mid - 1;
            mid = (hi + lo) / 2;
        }
    }
    return INVALID_PLACE;
}

PlaceMap* PlaceMap::load(const char* path) {
        gzFile placeMapgzf = gzopen(path, "rb");
        uint64_t version;
        uint64_t numrecords;
        int didRead;
#define READCHECK(N) if (didRead != (N)) { int gzerrno; fprintf(stderr, "%s: failed reading %s:%d %s\n", path, __FILE__, __LINE__, gzerror(placeMapgzf, &gzerrno)); return NULL; }
        didRead = gzread(placeMapgzf, &version, 8);
        READCHECK(8);
        if (version != 1) {
            fprintf(stderr, "%s: don't know how to handle version %lu\n", path, version);
            return NULL;
        }
        didRead = gzread(placeMapgzf, &numrecords, 8);
        READCHECK(8);

        ssize_t datalen = numrecords * 16;
        uint64_t* data = (uint64_t*)malloc(datalen);
        didRead = gzread(placeMapgzf, data, datalen);
        READCHECK(datalen);

        gzclose_r(placeMapgzf);
#undef READCHECK

        PlaceMap* out = new PlaceMap();
        out->data = data;
        out->numrecords = numrecords;
        return out;
};


using std::unique_ptr;

PlaceNames* PlaceNames::load(const char* path) {
    gzFile placeMapgzf = gzopen(path, "rb");
    if (placeMapgzf == NULL) {
        fprintf(stderr, "%s: could not gzopen\n", path);
        return NULL;
    }
    //int didRead;
    static const size_t buflen = 1000;
    unique_ptr<char> buffer((char*)malloc(buflen));
    std::vector<Place> places;
    int lineno = 0;
    int errcount = 0;
    static const int ERRLIMIT = 10;
    while (true) {
        if (errcount > ERRLIMIT) {
            fprintf(stderr, "%s: too many errors\n", path);
            break;
        }
        char* line = gzgets(placeMapgzf, buffer.get(), buflen);
        
        if (line == NULL) {
            break;
        }

        lineno++;
        int fieldno = 0;
        int fieldstart = 0;
        uint64_t placeNumber = 0;
        std::string name;
        std::string counties;
        int lp = 0;

        // get fields out of the line
        while (true) {
            switch (line[lp]) {
            case '|':
            case '\r':
            case '\n':
            case '\0':
                if (fieldno == 2) {
                    // place number
                    char* endp;
                    char* startp = line + fieldstart;
                    placeNumber = strtoull(startp, &endp, 10);
                    if (endp == startp) {
                        // Nothing converted
                        fprintf(stderr,"%s: bad line converting place number %d\n", path, lineno);
                        continue;
                    }
                } else if (fieldno == 3) {
                    // place name
                    name.append(line + fieldstart, lp - fieldstart);
                } else if (fieldno == 6) {
                    // counties
                    counties.append(line + fieldstart, lp - fieldstart);
                }
                fieldstart = lp + 1; // skip '|'
                fieldno++;
                break;
            default:
                ; // nop
            }

            if (line[lp] == '\0') {
                break;
            }
            lp++;
        }

        places.push_back(PlaceNames::Place(placeNumber, name, counties));
    }
    gzclose(placeMapgzf);
    if (errcount > ERRLIMIT) {
        return NULL;
    }
    std::sort(places.begin(), places.end());
    return new PlaceNames(places);
}

const PlaceNames::Place* PlaceNames::get(uint64_t place) const {
    int hi = places.size() - 1;
    int lo = 0;
    int mid = (hi + lo) / 2;
    while (lo <= hi) {
        uint64_t mv = places[mid].place;
        if (mv == place) {
            return &(places[mid]);
        } else if (mv < place) {
            lo = mid + 1;
            mid = (hi + lo) / 2;
        } else /* mv > place */ {
            hi = mid - 1;
            mid = (hi + lo) / 2;
        }
    }
    return NULL;
}
