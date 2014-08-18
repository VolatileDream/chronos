
#include "chronos.h"

// printf
#include <stdio.h>

// strncmp
#include <string.h>

static int usage( int argc, char** argv ){
	printf("usage: %s [init|count|list|get|append|iterate] \n\n", argv[0] );
	
	printf(" init <directory>                   - initialize a log in the directory\n" );
	printf(" count <directory>                  - prints the number of entries in the log\n");
	printf(" list <directory>                   - list all the keys in the log\n");
	printf(" get <directory> key                - get the specified key in the log\n");
	printf(" append <directory>                 - append a value to the log\n");
	printf(" iterate <directory> <s1> <s2> <s3> - prints out all the entries\n");
	
	printf("\n");

	printf("%s expects keys to parse to a date\n", argv[0]);
	printf("iterate prints out keys and entries as follows:\n");
	printf("    <s1>key<s2>entry<s3>\n");
	printf(" if any/all of s1, s2, or s3 are omitted, the empty string is printed instead\n");
	return 0;
}

static int init( int argc, char** argv ){

	char * dir = argv[2];

	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_only | cs_create, & handle );

	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	chronos_close( & handle );

	return 0;
}

static int count( int argc, char** argv ){

	char * dir = argv[2];

	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_only | cs_create, & handle );

	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	int count;
	rc = chronos_stat( & handle, & count, NULL );
	if( rc != 0 ){
		perror("chronos_stat");
		return rc;
	}

	chronos_close( & handle );

	printf("%d\n", count );

	return 0;
}

static int get( int argc, char** argv ){

	char * dir = argv[2];

	if( argc != 4 ){
		printf("bad number of arguments to get\n");
		return -1;
	}

	struct index_key key;
	int rc = parse_key( argv[3], strlen( argv[3] ), & key );
	if( rc != 0 ){
		printf("couldn't parse the key\n");
		return rc;
	}

	struct chronos_handle handle;
	rc = chronos_open( dir, cs_read_only | cs_create, & handle );
	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	struct index_entry entry;

	rc = chronos_find( & handle, & key, & entry );
	if( rc != 0 ){
		perror("chronos_find");
		return rc;
	}

	// output to stdout
	chronos_output( & handle, & entry, 1 );
	if( rc != 0 ){
		perror("chronos_output");
		return rc;
	}

	chronos_close( & handle );

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
	{ .name = "init", .func = &init },
	{ .name = "count", .func = &count },
	{ .name = "get", .func = &get },
/*
	{ .name = "list", .func = &list },
	{ .name = "append", .func = &append },
	{ .name = "iterate", .func = &iterate },
*/
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
