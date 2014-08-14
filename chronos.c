#include "chronos.h"

#include "headers.h"

int usage( int argc, char** argv ){
	printf("usage: %s [init|list|get|append] \n\n", argv[0] );
	
	printf(" init directory [keysize] - initialize a log in the directory,"
			" with the desired keysize (default 128 bytes)\n");
	printf(" list directory           - list all the keys in the log\n");
	printf(" get directory key        - get the specified key in the log\n");
	printf(" append directory         - append a value to the log\n");
	
	printf("\n");

	printf("%s expects keys to parse to a date\n", argv[0]);
	return 0;
}

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

static command_t commands[] = {
	{ .name = "--help", .func = &usage },
	{ .name = "-h", .func = &usage },
	{ .name = "?", .func = &usage },
	{ .name = "init", .func = &init },
	{ .name = "list", .func = &list },
	{ .name = "get", .func = &get },
	{ .name = "submit", .func = &append },
};

int main(int argc, char** argv){

	if( argc < 3 ){
		usage( argc, argv );
		return 2;
	}

	char* command = argv[1];

	for( int i=0; i < sizeof( commands ) / sizeof( command_t ); i++ ){
		// 10 because it should be longer than any command name in commands
		if( strncmp( command, commands[i].name, 10 ) == 0 ){
			return (*commands[i].func)( argc, argv );
		}
	}

	printf("error: unknown command '%s'\n", command );
	usage( argc, argv );
	return 2;
}

