#include "chronos.h"

#include "headers.h"

// read + write permissions for the user are default
#define FILE_PERMISSIONS ( S_IRUSR | S_IWUSR )

// create a file
static int create_file( char* dir, char* file ){
	char buffer[ PATH_BUFFER_SIZE ];

	snprintf( buffer, sizeof(buffer), "%s/%s", dir, file );

	int fd = creat( buffer, FILE_PERMISSIONS );

	if( fd == -1 ){
		return -1;
	}

	fd = close( fd );

	if( fd == -1 ){
		return -1;
	}

	return 0;
}
int init( int argc, char** argv ){

	char * dir = argv[2];

	int rc = mkdir( dir, FILE_PERMISSIONS | S_IXUSR ); // all permissions to user, none to anyone else

	if( rc != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nmkdir( dir ) failed\n", stderr);
		return -1;
	}

	int fd_dir = lock_dir( dir, 1 );

	if( fd_dir == -1 ){
		fputs("failed to grab directory lock\n", stderr);
		return -1;
	}

	// create the index + data files
	if( create_file( dir, INDEX_FILE ) != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfailed to create " INDEX_FILE "\n", stderr);
		return -1;
	}

	if( create_file( dir, DATA_FILE ) != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfailed to create " DATA_FILE "\n", stderr);
		return -1;
	}

	release_dir( fd_dir );

	return 0;
}
