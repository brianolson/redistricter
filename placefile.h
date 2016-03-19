#ifndef PLACEFILE_H
#define PLACEFILE_H

#include <stdint.h>
#include <string>
#include <vector>

/*
 * file of bigendian (uint64,uint64) mapping ubid to place number.
 */
class PlaceMap {
    public:
    static PlaceMap* load(const char* path);

    uint64_t placeForUbid(uint64_t);

    static uint64_t INVALID_PLACE;

    private:
    PlaceMap() {};
    uint64_t* data;
    uint64_t numrecords;
};

/*
 * '|' separated text lines, e.g.
 * OH|39|00464|Adena village|Incorporated Place|A|Harrison County, Jefferson County^M
 * {0: state}|{1: state fips number}|{2: place number}|{3: place name}|{4: place type text}|{5: place type code}|{6: place counties, comma separated}\r\n
 */
class PlaceNames {
    public:
    class Place {
        public:
        uint64_t place;
        std::string name;
        std::string counties;

        friend bool operator<(const Place& a, const Place& b) {
            return a.place < b.place;
        }
        Place(uint64_t place, std::string& name, std::string& counties)
            :place(place), name(name), counties(counties)
        {}
    };

    // sorted by place number for binary search
    std::vector<Place> places;

    static PlaceNames* load(const char* path);

    // may return NULL
    const Place* get(uint64_t place) const;

    private:
    PlaceNames(std::vector<Place>& placesIn)
        : places(placesIn)
    {}
};

#endif /* PLACEFILE_H */
