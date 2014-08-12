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
#define BUFFER_SIZE 256
// create a file
static int create_file( char* dir, char* file ){
	char buffer[ BUFFER_SIZE ];

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

#define INDEX_FILE "index"
#define DATA_FILE "data"

int init( int argc, char** argv ){

	char * dir = argv[2];

	int rc = mkdir( dir, FILE_PERMISSIONS | S_IXUSR ); // all permissions to user, none to anyone else

	if( rc != 0 ){
		fputs( strerror( errno ), stderr );
		fputs("\nmkdir( dir ) failed\n", stderr);
		return -1;
	}

	int fd_dir = get_dir_lock( dir, 1 );

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

static int open_file( char* dir, char* name, int will_write ){

	char buffer[ BUFFER_SIZE ];
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

#include <time.h>

static void print_key( struct index_key key ){
	// figure out how to format the key

	char buffer[1024];

	struct tm * time = gmtime( (time_t*) &key.seconds );

	// format the time
	ssize_t pos = strftime( buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time );

	// append microseconds to it
	pos = snprintf( buffer + pos, sizeof(buffer) - pos, ".%d", key.micros );

	fputs( buffer, stdout );
}

int list( int argc, char** argv ){
	char * dir = argv[2];

	int fd_dir = get_dir_lock( dir, 0 );

	if( fd_dir == -1 ){
		fputs("failed to grab directory lock\n", stderr);
		return -1;
	}

	int fd = open_file( dir, INDEX_FILE, 0 );

	if( fd == -1 ){
		fputs("failed to open index\n", stderr);
		return -1;
	}

	struct index_entry current;

	int rc = 0;

	while( (rc = read( fd, &current, sizeof(current))) ){
		if( rc != sizeof(current) ){
			// what happens here?
			// can this be ignored?
			fputs("Index appears to be broken.\n", stderr);
			return -1;
		}
		print_key( current.key );
		fputs("\n", stdout);
	}

	release_dir( fd_dir );

	return 0;
}

int get( int argc, char** argv ){
	//char * dir = argv[2];
	return 1;
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

	int fd_dir = get_dir_lock( dir, 1 );

	if( fd_dir == -1 ){
		fputs("failed to grab directory lock\n", stderr);
		return -1;
	}

	// open and write data file first,
	// then write the index update.
	int fd = open_file( dir, DATA_FILE, 1 );

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

	if( init_key( &entry.key ) == -1 ){
		fputs( strerror( errno ), stderr );
		fputs("\ninit_key( &entry.key ) failed\n", stderr);
		return -1;
	}

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

	// write the index entry
	fd = open_file( dir, INDEX_FILE, 1 );

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

