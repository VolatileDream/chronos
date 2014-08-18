#include "chronos.h"
#include "chronos-internal.h"

#include <stdlib.h>
#include <sys/mman.h>

// implementation of binary search.
static int compare( struct index_key * key1, struct index_key * key2 ){
	if( key1->seconds < key2->seconds ){
		return -1;
	}
	if( key1->seconds > key2->seconds ){
		return 1;
	}
	if( key1->nanos < key2->nanos ){
		return -1;
	}
	if( key1->nanos > key2->nanos ){
		return 1;
	}
	return 0;
}

static int find2( struct index_entry * entries, struct index_key * key, int start, int end, int count ){
	int mid = (end - start) / 2 + start;

	struct index_key * lookup = & entries[mid].key;

	int cmp = compare( key, lookup );
	if( cmp == 0 ){
		return mid;
	}

	// we ran out of search space, but didn't find the key
	if( mid == start || mid == end ){
		return -1;
	}

	if( cmp < 0 ){
		return find2( entries, key, start, mid, count );
	} else if( 0 < cmp ){
		return find2( entries, key, mid, end, count );
	}

	return -2;
}

static int find( struct index_entry * entries, struct index_key * key, int count ){
	return find2( entries, key, 0, count, count );
}

int chronos_find( struct chronos_handle * handle, struct index_key * key, struct index_entry * out_entry ){

	int rc = require_open_file( handle, cf_index, cs_read_only );
	if( rc != 0 ){
		return rc;
	}

	int entry_count;
	rc = chronos_stat( handle, & entry_count, NULL );

	void* mem_ptr = mmap( NULL // suggested segment offset
				, entry_count * sizeof(struct index_entry) // segment size
				, PROT_READ // segment execution/read/write settings
				, MAP_PRIVATE // mode
				, handle->index_fd // file descriptor
				, 0 ); // file offset

	if( mem_ptr == MAP_FAILED ){
		return C_READ_ERROR;
	}

	int index = find( (struct index_entry*)mem_ptr, key, entry_count );
	if( index >= 0 ){
		*out_entry = ((struct index_entry*)mem_ptr)[index];
	}

	munmap( mem_ptr, entry_count * sizeof(struct index_entry) );

	if( index < 0 ){
		return C_NOT_FOUND;
	} else {
		return 0;
	}
}
