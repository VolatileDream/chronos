#include "chronos.h"
#include "headers.h"

static void print_key( struct index_key key ){
	char buffer[1024];

	if( format_key( buffer, sizeof(buffer), & key ) != 0 ){
		fputs( buffer, stdout );
	}
}

int list( int argc, char** argv ){
	char * dir = argv[2];

	int fd_dir = lock_dir( dir, 0 );

	if( fd_dir == -1 ){
		fputs("failed to grab directory lock\n", stderr);
		return -1;
	}

	int fd = open_index( dir, 0, NULL );

	if( fd == -1 ){
		fputs("failed to open index\n", stderr);
		return -1;
	}

	struct index_entry current;

	int rc = 0;

	// TODO this loop is slow (small buffer size maybe?)

	while( (rc = read( fd, &current, sizeof(current))) ){
		if( rc != sizeof(current) ){
			// TODO this probably should have been detected earlier
			//  like when we attempted to write the first broken index
			fputs("Index appears to be broken.\n", stderr);
			return -1;
		}
		print_key( current.key );
		fputs("\n", stdout);
	}

	release_dir( fd_dir );

	return 0;
}
