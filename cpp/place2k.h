#ifndef PLACES_2K_H
#define PLACES_2K_H

#define ZCTA_STATE_OFFSET ((int)0)
#define ZCTA_NAME_OFFSET ((int)2)
#define ZCTA_TOTAL_POPULATION_OFFSET ((int)66)
#define ZCTA_TOTAL_HOUSING_UNITS_OFFSET ((int)75)
#define ZCTA_LAND_AREA_SQ_M_OFFSET ((int)84)
#define ZCTA_WATER_AREA_SQ_M_OFFSET ((int)98)
#define ZCTA_LAND_AREA_SQ_MI_OFFSET ((int)112)
#define ZCTA_WATER_AREA_SQ_MI_OFFSET ((int)124)
#define ZCTA_LATITUDE_OFFSET ((int)136)
#define ZCTA_LONGITUDE_OFFSET ((int)146)
#define ZCTA_CR_OFFSET ((int)157)
#define ZCTA_NEXT_OFFSET ((int)159)
#define sizeof_ZCTA ((int)159)

#define copyZCTAField( dst, src, i, start, stop ) memcpy( dst, (void*)((unsigned long)src + i*sizeof_ZCTA + start ), stop - start ); ((unsigned char*)dst)[stop-start] = '\0';

//#define copyName( dst, src, i ) copyZCTAField( dst, src, i, , )
#define copyName( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_NAME_OFFSET, ZCTA_TOTAL_POPULATION_OFFSET )
#define copyZip( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_NAME_OFFSET, (ZCTA_NAME_OFFSET+5) )
#define copyLongitude( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_LONGITUDE_OFFSET, ZCTA_CR_OFFSET )
#define copyLatitude( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_LATITUDE_OFFSET, ZCTA_LONGITUDE_OFFSET )
#define copyPopulation( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_TOTAL_POPULATION_OFFSET, ZCTA_TOTAL_HOUSING_UNITS_OFFSET )
#define copyLandAreaMM( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_LAND_AREA_SQ_M_OFFSET, ZCTA_WATER_AREA_SQ_M_OFFSET )
#define copyWaterAreaMM( dst, src, i ) copyZCTAField( dst, src, i, ZCTA_WATER_AREA_SQ_M_OFFSET, ZCTA_LAND_AREA_SQ_MI_OFFSET )


#endif
