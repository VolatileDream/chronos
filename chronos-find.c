#include "chronos.h"

#include <stdlib.h>
#include <sys/mman.h>

// min macro from: https://stackoverflow.com/questions/3437404/min-and-max-in-c
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// implementation of binary search.

static int find2( struct chronos_handle * handle, struct index_key * key, int start, int end, int count ){
	int mid = (end - start) / 2 + start;

	struct index_entry lookup;

	int rc = chronos_entry( handle, mid, & lookup );
	if( rc != 0 ){
		return -rc;
	}

	int cmp = index_key_cmp( key, & lookup.key );
	if( cmp == 0 ){
		return mid;
	}

	// we ran out of search space, but didn't find the key
	if( mid == start || mid == end ){
		return -C_NOT_FOUND;
	}

	if( cmp < 0 ){
		return find2( handle, key, start, mid, count );
	} else if( 0 < cmp ){
		return find2( handle, key, mid, end, count );
	}

	return -2;
}

static int find( struct chronos_handle * handle, struct index_key * key, int count ){
	return find2( handle, key, 0, count, count );
}

int chronos_find( struct chronos_handle * handle, struct index_key * key, struct index_entry * out_entry ){

	int entry_count = 0;
	int rc = chronos_stat( handle, & entry_count, NULL );
	if( rc != 0 ){
		return rc;
	}

	// this case causes mmap to error
	if( entry_count == 0 ){
		return C_NOT_FOUND;
	}

	int index = find( handle, key, entry_count );
	if( index >= 0 ){
		rc = chronos_entry( handle, index, out_entry );
	} else {
		rc = -index;
	}

	if( index < 0 ){
		return C_NOT_FOUND;
	} else if ( rc != 0 ){
		return rc;
	} else {
		return 0;
	}
}
