#include "chronos.h"

#include "headers.h"

// for gettimeofday
#include <sys/time.h>

int init_key( struct index_key * key ){

	struct timeval time;

	int rc = gettimeofday( &time, NULL );

	if( rc == -1 ){
		return -1;
	}

	key->seconds = time.tv_sec ;
	key->micros = time.tv_usec % (1000 * 1000) ; // microseconds off the second

	return 0;
}

// copies everything from 'fd_from' and writes it to 'fd_to'
static int copy_fd( int fd_from, int fd_to, uint32_t *out_size ){

	uint32_t length = 0;

	char buffer[ 1048 ];

	ssize_t read_length = 0;

	// read from stdin
	while( (read_length = read( fd_from, buffer, sizeof(buffer) )) ){

		if( read_length == -1 ){
			return -1;
		}

		// keep updating the length of the entry
		length += read_length;

		ssize_t wrote = 0;
		while( read_length - wrote > 0 ){
			int rc = write( fd_to, buffer + wrote, read_length - wrote );
			if( rc == -1 ){
				return -1;
			}
			wrote += rc;
		}
	}

	*out_size = length;

	return 0;
}

int append( int argc, char** argv ){
	char * dir = argv[2];

	int fd_dir = lock_dir( dir, 1 );

	if( fd_dir == -1 ){
		fputs("failed to grab directory lock\n", stderr);
		return -1;
	}

	// open and write data file first,
	// then write the index update.
	int fd = open_data( dir, 1 );

	if( fd == -1 ){
		fputs("failed to open data file\n", stderr);
		return -1;
	}

	// Get the size of the file, the directory
	// is locked. So it won't change on us.
	struct stat info;

	int rc = fstat( fd, &info );

	if( rc == -1 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfstat( fd, &info ) failed\n", stderr);
		return -1;
	}

	struct index_entry entry = {
		.position = info.st_size,
		.length = 0, // only temporary
	};

	// copy stdin to the data, and get the entry length
	rc = copy_fd( 0, fd, &entry.length );

	if( rc == -1 ){
		fputs( strerror( errno ), stderr );
		fputs("\ncopy_fd( stdin, data, entry.length ) failed\n", stderr);
		return -1;
	}

	// don't care if they error.
	close( 0 ); // empty
	close( fd ); // finished using

	// TODO should this be before or after we insert the key?
	//  that is, should it be closer to the index insertion, or the time
	//  that we attempted to put the data into the store?
	// finish key init, this step binds the time to the key.
	if( init_key( &entry.key ) == -1 ){
		fputs( strerror( errno ), stderr );
		fputs("\ninit_key( &entry.key ) failed\n", stderr);
		return -1;
	}

	// write the index entry
	fd = open_index( dir, 1, NULL );

	if( fd == -1 ){
		fputs("failed to open index\n", stderr);
		return -1;
	}

	// TODO we should probably stat the file first, to make sure
	// this entry is aligned correctly when written.
	// (that is that it will get read correctly when we come back later)
	rc = write( fd, &entry, sizeof(entry) );

	if( rc == -1 ){
		fputs("failed write to index\n", stderr);
		return -1;
	}

	close( fd );

	release_dir( fd_dir );

	return 0;
}
