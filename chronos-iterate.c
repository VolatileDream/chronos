#include "chronos.h"

#include "headers.h"

int write_delim( int fd_out, char* delim, int len ){
	if( len > 0){
		int rc = write( fd_out, delim, len );

		if( rc != len ){
			fputs("error writing delim.\n", stderr);
			return -1;
		}
	}
	return 0;
}

int iterate( int argc, char** argv ){
	char * dir = argv[2];

	int before_key_len, before_entry_len, after_entry_len;
	char *before_key, *before_entry, *after_entry;

	before_key_len = before_entry_len = after_entry_len = 0;
	before_key = before_entry = after_entry = NULL;

	if( argc > 3 ){
		before_key = argv[3];
		before_key_len = strlen( before_key );
		if( argc > 4 ){
			before_entry = argv[4];
			before_entry_len = strlen( before_entry );
			if( argc > 5 ){
				after_entry = argv[5];
				after_entry_len = strlen( after_entry );
			}
		}
	}

	//printf("iterate: %d\n", argc);

	//printf("args: s1 '%s', s2 '%s', s3 '%s'\n", before_key, before_entry, after_entry );

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

	const int fd_out = 1;

	int rc;
	while( (rc = read( fd_index, &cur_entry, sizeof(cur_entry))) ){
		if( rc != sizeof(cur_entry) ){
			fputs("error reading index.\n", stderr);
			return -1;
		}

		// because of buffering problems with fputs we have to use write everywhere.
		//  this is because write_out uses write -> these problems

		if( write_delim( fd_out, before_key, before_key_len ) != 0 ){
			return -1;
		}

		// now format + print the key
		char buffer[1024];
		int key_len = format_key( buffer, sizeof(buffer), & cur_entry.key );
		rc = write( fd_out, buffer, key_len );

		if( rc != key_len ){
			fputs("error writing key.\n", stderr);
			return -1;
		}

		if( write_delim( fd_out, before_entry, before_entry_len ) != 0 ){
			return -1;
		}

		// this is likely to end up being monotonically increasing.
		// however, is someone decides to be fancy and compact the data store
		//  then we have to iterate the entries in this way, because it's what
		//  is expected by the data store and index format.
		lseek( fd_data, cur_entry.position, SEEK_SET );

		rc = write_out( fd_data, fd_out, cur_entry.length );
		if( rc == -1 ){
			fputs("failed to write data out\n", stderr);
			return -1;
		}

		if( write_delim( fd_out, after_entry, after_entry_len ) != 0 ){
			return -1;
		}
	}


	close( fd_index );

	close( fd_data );

	release_dir(fd_dir);

	return 0;
}
