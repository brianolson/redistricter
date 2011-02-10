#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// storage floating type
#ifndef SFTYPE
#define SFTYPE double
#endif
// just has to be big enough to identify a district number. unsigned char good up through 255 districts
#ifndef POPTYPE
#define POPTYPE uint8_t
#define POPTYPE_MAX 0xfc
#endif

#ifndef NODISTRICT
#define NODISTRICT 0xff
#endif

#ifndef ERRDISTRICT
#define ERRDISTRICT 0xfe
#endif

#ifndef WITH_PNG
#if NOPNG
#define WITH_PNG 0
#else
#define WITH_PNG 1
#endif
#endif

// see GeoData.h for the usage of the next batch of defines
#ifndef COUNT_DISTRICTS
#define COUNT_DISTRICTS 1
#endif

#endif /* CONFIG_H */
