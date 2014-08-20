#include "chronos.h"
#include "chronos-internal.h"

#include <stdint.h>
#include <unistd.h>

// copies everything from 'fd_from' and writes it to 'fd_to'
static int copy_fd( int fd_from, int fd_to, uint32_t *out_size ){

	uint32_t length = 0;

	char buffer[ 1048 ];
	ssize_t read_length = 0;

	// read from stdin
	while( (read_length = read( fd_from, buffer, sizeof(buffer) )) ){

		if( read_length == -1 ){
			return C_IO_READ_ERROR;
		}

		// keep updating the length of the entry
		length += read_length;

		ssize_t wrote = 0;
		while( read_length - wrote > 0 ){
			int rc = write( fd_to, buffer + wrote, read_length - wrote );
			if( rc == -1 ){
				return C_IO_WRITE_ERROR;
			}
			wrote += rc;
		}
	}
	*out_size = length;
	return 0;
}

int chronos_append( struct chronos_handle * handle, struct index_key * maybe_key, int fd_in ){

	int rc;

	uint32_t size;
	rc = chronos_stat( handle, NULL, & size );
	if( rc != 0 ){
		return rc;
	}

	struct index_entry entry;

	entry.position = size;

	if( maybe_key ){

		entry.key = * maybe_key;

	} else {
		rc = init_key( & entry.key );
		if( rc ){
			return rc;
		}
	}

	rc = require_open_file( handle, cf_data_store, cs_read_write );
	if( rc != 0 ){
		return rc;
	}

	uint32_t total_length;
	rc = copy_fd( fd_in, handle->data_fd, & total_length );

	if( rc != 0 ){
		return rc;
	}

	entry.length = total_length;

	// now write to the index file.
	rc = require_open_file( handle, cf_index, cs_read_write );
	if( rc != 0 ){
		return rc;
	}

	rc = write( handle->index_fd, & entry, sizeof( entry ) );
	if( rc == -1 ){
		return C_IO_WRITE_ERROR;
	}

	// make sure that all the changes are synced to disk
	fsync( handle->dir_fd );

	return 0;
}