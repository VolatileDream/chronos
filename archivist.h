#ifndef __ARCHIVIST_H__
#define __ARCHIVIST_H__

#include <stdint.h>

struct index_header {
	// does this need anything?
};

struct index_key {
	uint32_t seconds; // seconds past epoch
	uint32_t micros; // microseconds past epoch, discounting seconds
};

struct index_entry {
	uint32_t position;
	uint32_t length;
	struct index_key key;
};

struct data_entry {
	uint8_t * value;
};

#endif /* __ARCHIVIST_H__ */
