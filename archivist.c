#include "archivist.h"

// for the errno variable
#include <errno.h>

// for exit
#include <stdlib.h>
// for printf
#include <stdio.h>
// for strncmp
#include <string.h>

typedef int (*command_func)( int argc, char** argv );

typedef struct command_t {
	const char* name;
	const command_func func;
} command_t;


static int usage( int argc, char** argv ){
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

// for gettimeofday
#include <sys/time.h>

void init_key( struct index_key * key ){

	struct timeval time;

	int rc = gettimeofday( &time, NULL );

	if( rc == -1 ){
		exit(errno);
	}

	key->seconds = time.tv_sec ;
	key->micros = time.tv_usec % (1000 * 1000) ; // microseconds off the second
}

// for file/dir open, close and flags
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

// returns the fd of the locked directory, or -1 and an error is printed to stderr
static int get_dir_lock( char* dir, int will_write ){

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

static int release_dir(int fd_dir){
	return flock( fd_dir, LOCK_UN );
}

// read, write and execute for the user
#define FILE_PERMISSIONS ( S_IRUSR | S_IWUSR )

// create a file
static int create_file( char* dir, char* file ){
	char buffer[256];

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

	int fd_dir = get_dir_lock( argv[2], 1 );

	if( fd_dir == -1 ){
		fputs("failed to grab directory lock\n", stderr);
		return -1;
	}

	// create the index + data files
	if( create_file( dir, "index" ) != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfailed to create index\n", stderr);
		return -1;
	}

	if( create_file( dir, "data" ) != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nfailed to create date\n", stderr);
		return -1;
	}

	release_dir( fd_dir );

	return 0;
}

int list( int argc, char** argv ){
	return 1;
}

int get( int argc, char** argv ){
	return 1;
}

int append( int argc, char** argv ){
	return 1;
}

static command_t commands[] = {
	{ .name = "--help", .func = &usage },
	{ .name = "-h", .func = &usage },
	{ .name = "?", .func = &usage },
	{ .name = "init", .func = &init },
	{ .name = "list", .func = &list },
	{ .name = "get", .func = &get },
	{ .name = "append", .func = &append },
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

