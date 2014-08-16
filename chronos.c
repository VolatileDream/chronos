#include "chronos.h"

#include "headers.h"

#include <time.h>

// returns the fd of the locked directory, or -1 and an error is printed to stderr
int lock_dir( char* dir, int will_write ){

	int fd_dir = open( dir, O_RDONLY );

	if( fd_dir == -1 ){
		fputs( strerror( errno ), stderr );
		fputs("\nopen( dir ) failed\n", stderr);
		return -1;
	}

	struct stat dir_data;

	// ensure the directory exists + create it if it doesn't
	int rc = fstat( fd_dir, &dir_data );

	if( rc != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfstat( fd_dir, &dir_data ) failed\n", stderr);
		return -1;
	}

	if( !S_ISDIR( dir_data.st_mode ) ){
		fputs( "error: path provided was not a directory\n", stderr);
		return -1;
	}

	int lock = LOCK_SH;
	if( will_write ){
		lock = LOCK_EX;
	}

	if( flock( fd_dir, lock ) ){
		fputs( strerror( errno ), stderr );
		fputs("\nflock( fd_dir, lock ) failed\n", stderr);
		return -1;
	}

	return fd_dir;
}

int release_dir(int fd_dir){
	return flock( fd_dir, LOCK_UN );
}


static int open_file( char* dir, char* name, int will_write ){

	char buffer[ PATH_BUFFER_SIZE ];
	snprintf( buffer, sizeof(buffer), "%s/%s", dir, name );

	int flags = O_RDONLY;
	if( will_write ){
		flags = O_RDWR | O_APPEND;
	}

	int fd = open( buffer, flags );

	if( fd == -1 ){
		fputs( strerror( errno ), stderr );
		fputs("\nopen( ) failed\n", stderr);
		return -1;
	}

	return fd;
}

int open_index( char* dir, int will_write, int * out_index_count ){
	int fd = open_file( dir, INDEX_FILE, will_write );

	if( fd == -1 ){
		return -1;
	}

	// do a bunch of checks to ensure that the index is healthy

	struct stat index_data;

	// ensure the directory exists + create it if it doesn't
	int rc = fstat( fd, &index_data );

	if( rc != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfstat( fd_dir, &dir_data ) failed\n", stderr);
		return -1;
	}

	if( index_data.st_size % sizeof(struct index_entry) != 0 ){
		// this is a serious problem. how do we handle it?
		fputs("Index size is bad not a multiple of sizeof(struct index_entry).\n", stderr);
		return -1;
	}

	if( out_index_count != NULL ){
		* out_index_count = index_data.st_size / sizeof( struct index_entry );
	}

	return fd;
}

int open_data( char* dir, int will_write ){
	// this gets passed through because we have
	// no fancy checks to do at this time. 
	return open_file( dir, DATA_FILE, will_write );
}

#define KEY_FORMAT "%Y-%m-%d %H:%M:%S"

int format_key( char * buf, int max, struct index_key * key ){

	struct tm * time = gmtime( (time_t*) &key->seconds );

	int strf_count = strftime( buf, max, KEY_FORMAT, time );

	if( strf_count == 0 ){
		// not enough space, can't do anything about that, need a larger buffer
		return 0;
	}

	// append nanoseconds to it
	int sn_count = snprintf( buf + strf_count, max - strf_count, ".%09d", key->nanos );

	if( sn_count == 0 ){
		// couldn't add the micros to the string
		return 0;
	}

	return strf_count + sn_count;
}

int parse_key( char * str, int length, struct index_key * out_key ){

	struct tm time;

	char * end_of_date = strptime( str, KEY_FORMAT, &time );

	if( end_of_date == NULL ){
		// this is a failure to parse the string
		fputs("Error parsing key, date segment was invalid.\n", stderr);
		return -1;
	}

	// convert to number of seconds since epoch
	out_key->seconds = timegm( &time );

	char * end;
	// +1 because the start of the string will be a '.' 
	out_key->nanos = strtol( end_of_date + 1, &end, 10 );

	if( *end != 0 ){ // check that strtol ate until the end of the string
		// couldn't parse until the end of the string
		fputs("Error parsing key, nanos were invalid\n", stderr);
		return -1;
	}

	return 0;
}

int write_out( int fd_in, int fd_out, int count ){
	char buffer[1024];

	while( count > 0 ){
		int size = min( sizeof(buffer), count );

		int read_count = read( fd_in, buffer, size );

		if( read_count == -1 ){
			fputs( strerror( errno ), stderr );
			fputs("\nread from data failed\n", stderr);
			return -1;
		}

		int wrote = 0;

		while( wrote < read_count ){
			int rc = write( fd_out, buffer + wrote, read_count - wrote );
			if( rc == -1 ){
				fputs( strerror( errno ), stderr );
				fputs("\nread from data failed\n", stderr);
				return -1;
			}
			wrote += rc;
		}

		count -= read_count;
	}

	return 0;
}
