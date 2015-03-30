#include "chronos.h"
#include "chronos-internal.h"

#include <stdint.h>
#include <unistd.h>

// splice
#include <fcntl.h>

// copies everything from 'fd_from' and writes it to 'fd_to'
static int copy_fd( int fd_from, int fd_to, loff_t offset, uint32_t *out_size ){

	uint32_t length = 0;

	// read from stdin
	while( 1 ){

		ssize_t read = splice( fd_from, NULL, fd_to, &offset, 0xffffffff, 0 );

		if( read == 0 ){
			break;
		}

		if( read == -1 ){
			return C_IO_READ_ERROR;
		}

		// keep updating the length of the entry
		length += read;
	}

	*out_size = length;
	return 0;
}

static int key_is_later( struct chronos_handle * handle, struct index_key * provided ){

	// bounds check on key date-time
	struct index_entry last_entry;

	int rc = chronos_entry( handle, -1, & last_entry );

	if( rc == C_NO_MORE_ELEMENTS ){
		// empty index means the key is always later
		return 0;

	} else if( rc != 0 ){
		return rc;
	}

	if( index_key_cmp( & last_entry.key, provided ) >= 0 ){
		return C_PROVIDED_KEY_NOT_LATEST;
	}

	return 0;
}

int chronos_append( struct chronos_handle * handle, struct index_key * maybe_key, int fd_in ){

	int rc;

	uint32_t size = 0;
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

	rc = key_is_later( handle, & entry.key );
	if( rc != 0 ){
		return rc;
	}

	rc = require_open_file( handle, cf_data_store, cs_read_write );
	if( rc != 0 ){
		return rc;
	}

	uint32_t total_length;
	rc = copy_fd( fd_in, handle->data_fd, size, & total_length );

	if( rc != 0 ){
		return rc;
	}

	entry.length = total_length;

	// now write to the index file.
	rc = require_open_file( handle, cf_index, cs_read_write );
	if( rc != 0 ){
		return rc;
	}

	struct index_entry platform_agnostic;

	make_platform_agnostic( & entry, & platform_agnostic );

	rc = write( handle->index_fd, & platform_agnostic, sizeof( platform_agnostic ) );
	if( rc == -1 ){
		return C_IO_WRITE_ERROR;
	}

	// make sure that all the changes are synced to disk
	fsync( handle->dir_fd );

	return 0;
}

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

#include <arpa/inet.h>

// basic serialization protocol to be platform agnostic
void make_platform_agnostic( struct index_entry * entry, struct index_entry * out_platform_agnostic ){
	out_platform_agnostic->position    = htonl( entry->position );
	out_platform_agnostic->length      = htonl( entry->length );
	out_platform_agnostic->key.seconds = htonl( entry->key.seconds );
	out_platform_agnostic->key.nanos   = htonl( entry->key.nanos );
}

void make_platform_specific( struct index_entry * out_entry, struct index_entry * platform_agnostic ){
	out_entry->position    = ntohl( platform_agnostic->position );
	out_entry->length      = ntohl( platform_agnostic->length );
	out_entry->key.seconds = ntohl( platform_agnostic->key.seconds );
	out_entry->key.nanos   = ntohl( platform_agnostic->key.nanos );
}

