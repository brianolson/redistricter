#ifndef SWAP_H
#define SWAP_H

static inline uint16_t swap16( uint16_t v ) {
	return ((v >> 8) &  0x00ff) |
	((v << 8) & 0xff00);
}
static inline uint32_t swap32( uint32_t v ) {
	return ((v >> 24) & 0xff) |
	((v >> 8) & 0xff00) |
	((v & 0xff00) << 8) |
	((v & 0xff) << 24);
}
static inline uint64_t swap64( uint64_t v ) {
	return
	((v & 0xff00000000000000ULL) >> 56) |
	((v & 0x00ff000000000000ULL) >> 40) |
	((v & 0x0000ff0000000000ULL) >> 24) |
	((v & 0x000000ff00000000ULL) >>  8) |
	((v & 0x00000000ff000000ULL) <<  8) |
	((v & 0x0000000000ff0000ULL) << 24) |
	((v & 0x000000000000ff00ULL) << 40) |
	((v & 0x00000000000000ffULL) << 56);
}

#endif /* SWAP_H */
