#include "chronos.h"

#include "headers.h"

int iterate( int argc, char** argv ){
	char * dir = argv[2];

	int fd_dir = lock_dir( dir, 0 );

	if( fd_dir == -1 ){
		return -1;
	}

	int entry_count = 0;
	int fd_index = open_index( dir, 0, &entry_count );

	if( fd_index == -1 ){
		fputs("couldn't open index.\n", stderr);
		return -1;
	}

	int fd_data = open_data( dir, 0 );

	if( fd_data == -1 ){
		fputs("couldn't open data.\n", stderr);
		return -1;
	}

	struct index_entry cur_entry;

	int rc;
	while( (rc = read( fd_index, &cur_entry, sizeof(cur_entry))) ){
		if( rc != sizeof(cur_entry) ){
			fputs("error reading index.\n", stderr);
			return -1;
		}
		// this is likely to end up being monotonically increasing.
		// however, is someone decides to be fancy and compact the data store
		//  then we have to iterate the entries in this way, because it's what
		//  is expected by the data store and index format.
		lseek( fd_data, cur_entry.position, SEEK_SET );

		int rc = write_out( fd_data, 1, cur_entry.length );
		if( rc == -1 ){
			fputs("failed to write data out\n", stderr);
			return -1;
		}
	}


	close( fd_index );

	close( fd_data );

	release_dir(fd_dir);

	return 0;
}
