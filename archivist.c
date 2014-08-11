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

int init( int argc, char** argv ){
	return 1;
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

	if( argc < 2 ){
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

