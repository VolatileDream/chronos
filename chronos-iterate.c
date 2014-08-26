#include "chronos.h"
#include "chronos-internal.h"

#include <unistd.h>

int chronos_entry( struct chronos_handle * handle, int entry_number, struct index_entry * out ){

	int rc = require_open_file( handle, cf_index, cs_read_only );
	if( rc != 0 ){
		return rc;
	}

	int count = 0;

	rc = chronos_stat( handle, & count, NULL );
	if( rc != 0 ){
		return rc;
	}

	if( entry_number < 0 ){
		entry_number += count;
	}

	// bounds check
	if( entry_number < 0 || entry_number >= count ){
		return C_NO_MORE_ELEMENTS;
	}

	int desired_position = entry_number * sizeof(*out);

	int offset = lseek( handle->index_fd, desired_position, SEEK_SET );
	if( offset != desired_position ){
		// error seeking to the position
		return C_NO_MORE_ELEMENTS;
	}

	if( offset == -1 ){
		return C_IO_READ_ERROR;
	}

	struct index_entry platform_agnostic;

	int read_data = read( handle->index_fd, & platform_agnostic, sizeof(platform_agnostic) );
	if( read_data == -1 || read_data < sizeof(platform_agnostic) ){
		return C_IO_READ_ERROR;
	}

	make_platform_specific( out, & platform_agnostic );

	return 0;
}

int chronos_iterate( struct chronos_handle * handle, struct chronos_iterator * out_iter ){

	int count = 0;

	int rc = chronos_stat( handle, & count, NULL );
	if( rc != 0 ){
		return rc;
	}

	out_iter->index_position = 0;
	out_iter->index_size = count;

	return 0;
}

int chronos_iterate_next( struct chronos_handle * handle, struct chronos_iterator * iter, struct index_entry * out ){

	if( iter->index_position >= iter->index_size ){
		return C_NO_MORE_ELEMENTS;
	}

	int rc = chronos_entry( handle, iter->index_position, out );
	if( rc != 0 ){
		return rc;
	}

	// and lastly update the iterator position
	iter->index_position++;

	return 0;
}

