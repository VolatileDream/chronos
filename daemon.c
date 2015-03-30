
#include "chronos.h"

#include <stdint.h>

// printf
#include <stdio.h>

// exit
#include <stdlib.h>

// strncmp
#include <string.h>

// write
#include <unistd.h>
// splice
#include <fcntl.h>

// mkfifo
#include <sys/types.h>
#include <sys/stat.h>

static int usage( int argc, char** argv ){
	printf("usage: %s <command> [args ...] \n\n", argv[0] );

	printf("Commands:\n");
	
	printf(" daemon <directory>\n");
	printf("   start a daemon running for the chronos instance\n\n");

	printf(" append <directory>\n");
	printf("   append a value to the log\n");
	printf("   -t allows the user to change the time that the entry reports being inserted.\n");

	return 0;
}

static int fifo_get_name( char* buffer, size_t len, char* directory ){
	return snprintf(buffer, len, "%s/fifo", directory );
}

static int copy_fd( int fd_from, int fd_to ){
	// read from stdin
	while( 1 ){

		ssize_t read = splice( fd_from, NULL, fd_to, NULL, 0xffffffff, 0 );

		if( read == 0 ){
			break;
		}

		if( read == -1 ){
			return C_IO_READ_ERROR;
		}
	}
	return 0;
}

static int do_append( int argc, char ** argv ){

	char* dir = argv[2];

	char fifo_name[ PATH_BUFFER_SIZE ];

	// insert the chronos directory that we want, and then the fifo file
	int rc = fifo_get_name( fifo_name, sizeof(fifo_name), dir );

	// truncated output	
	if( rc > sizeof(fifo_name) ){
		return rc;
	}

	int fd = open( fifo_name, O_WRONLY );

	if( fd == -1 ){
		perror("append open");
		return rc;
	}

	// stdin -> opened fifo
	rc = copy_fd( 0, fd );

	if( rc != 0 ){
		perror("append copy");
		return rc;
	}

	close(fd);

	return 0;
}

static int do_daemon( int argc, char** argv ){

	char* dir = argv[2];
	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_write, & handle );
	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	char fifo_name[ PATH_BUFFER_SIZE ];

	// insert the chronos directory that we want, and then the fifo file
	rc = fifo_get_name( fifo_name, sizeof(fifo_name), dir );

	// truncated output	
	if( rc > sizeof(fifo_name) ){
		return rc;
	}

	rc = mkfifo(fifo_name, 0770 );

	if( rc != 0 ){
		perror("daemon mkfifo");
		return rc;
	}

	while(1){
		int fd = open( fifo_name, O_RDONLY );
		if( fd < 0 ){
			perror("fifo open");
			break;
		}

		rc = chronos_append( &handle, NULL, fd );

		if( rc != 0 ){
			perror("fifo append");
		}

		close(fd);
	}

	rc = unlink(fifo_name);

	chronos_close( &handle );

	return 0;
}

// commands for the cli interface
typedef int (*command_func)( int argc, char** argv );

typedef struct command_t {
       const char* name;
       const command_func func;
} command_t;

static command_t commands[] = {
	{ .name = "--help", .func = &usage },
	{ .name = "-h", .func = &usage },
	{ .name = "?", .func = &usage },
	{ .name = "append", .func = &do_append },
	{ .name = "daemon", .func = &do_daemon },
};

int main(int argc, char** argv){

	if( argc < 3 ){
		fprintf( stderr, "insufficient arguments supplied, require at least command and directory.\n" );
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

	fprintf( stderr, "error: unknown command '%s'\n", command );
	usage( argc, argv );
	return 2;
}
