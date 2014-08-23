
#include "chronos.h"

// printf
#include <stdio.h>

// strncmp
#include <string.h>

// write
#include <unistd.h>

static int usage( int argc, char** argv ){
	printf("usage: %s <command> [args ...] \n\n", argv[0] );

	printf("Commands:\n");
	
	printf(" init <directory>\n");
	printf("   initialize a log in the directory\n\n");

	printf(" count <directory>\n");
	printf("   prints the number of entries in the log\n\n");

	printf(" last <directory>\n");
	printf("   prints out the last key in the log\n\n");

	printf(" list <directory>\n");
	printf("   list all the keys in the log\n\n");

	printf(" get <directory> <key>\n");
	printf("   get the specified key in the log\n\n");

	printf(" append <directory> [-t <key>]\n");
	printf("   append a value to the log\n");
	printf("   -t allows the user to change the time that the entry reports being inserted.\n\n");

	printf(" iterate <directory> <s1> <s2> <s3>\n");
	printf("   prints out keys and entries as follows:\n");
	printf("     <s1>key<s2>entry<s3>\n");
	printf("   if any/all of s1, s2, or s3 are omitted, the empty string is printed instead\n");
	
	printf("\n");

	printf("%s expects keys to parse to a date\n", argv[0]);
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

	int rc = chronos_open( dir, cs_read_only, & handle );

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
	rc = chronos_open( dir, cs_read_only, & handle );
	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	struct index_entry entry;

	rc = chronos_find( & handle, & key, & entry );
	if( rc != 0 ){
		if( rc != C_NOT_FOUND ){
			perror("chronos_find");
		}
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

static int append( int argc, char** argv ){

	char * dir = argv[2];

	int key_parsed = 0;
	struct index_key key;

	if( argc > 4 && strncmp("-t", argv[3], 2) == 0 ){
		int rc = parse_key( argv[4], strlen( argv[4] ), & key );
		if( rc == 0 ){
			key_parsed = 1;
		} else {
			printf("bad key value: %s\n", argv[4]);
			return rc;
		}
	}

	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_only, & handle );

	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}
	if( key_parsed ){
		rc = chronos_append( & handle, & key, 0 );
	} else {
		rc = chronos_append( & handle, NULL, 0 );
	}
	if( rc != 0 ){
		if( rc == C_PROVIDED_KEY_NOT_LATEST ){
			printf("can't insert keys before the last entry.\n");
		} else {
			perror("chronos_append");
		}
		return rc;
	}

	chronos_close( & handle );

	return 0;
}

static int last( int argc, char** argv ){

	char * dir = argv[2];

	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_only, & handle );

	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	struct index_entry entry;

	rc = chronos_entry( & handle, -1, & entry );
	if( rc != 0 ){
		if( rc != C_NO_MORE_ELEMENTS ){
			perror("chronos_append");
		}
		return rc;
	}

	char buffer[1024];
	format_key( buffer, sizeof(buffer), & entry.key );
	printf( "%s\n", buffer );

	chronos_close( & handle );

	return 0;
}
typedef void (*ui_iterator)( int argc, char** argv, struct chronos_handle * handle, struct index_entry * entry );

static int do_iterate( int argc, char** argv, ui_iterator ui_iter ){

	char * dir = argv[2];

	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_only, & handle );

	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	struct chronos_iterator iter;
	rc = chronos_iterate( & handle, & iter );
	if( rc != 0 ){
		perror("chronos_iterate");
		return rc;
	}

	struct index_entry entry;

	for(;;){
		rc = chronos_iterate_next( & handle, & iter, & entry );
		if( rc != 0 ){
			break;
		}
		ui_iter( argc, argv, & handle, & entry );
	}

	if( rc == C_NO_MORE_ELEMENTS ){
		rc = 0;
	}

	chronos_close( & handle );

	return rc;
}

void list_iter( int argc, char** argv, struct chronos_handle * handle, struct index_entry * entry ){
	char buffer[1024];
	format_key( buffer, sizeof(buffer), & entry->key );
	printf("%s\n", buffer);
}
int list( int argc, char** argv ){
	return do_iterate( argc, argv, & list_iter );
}

void full_iter( int argc, char** argv, struct chronos_handle * handle, struct index_entry * entry ){
	char buffer[1024];
	int length = format_key( buffer, sizeof(buffer), & entry->key );

	if( argc > 3 ){
		write( 1, argv[3], strlen( argv[3] ) );
	}

	write( 1, buffer, length );

	if( argc > 4 ){
		write( 1, argv[4], strlen( argv[4] ) );
	}

	chronos_output( handle, entry, 1 );

	if( argc > 5 ){
		write( 1, argv[5], strlen( argv[5] ) );
	}
}
int iterate( int argc, char** argv ){
	printf("%d\n", argc);
	return do_iterate( argc, argv, & full_iter );
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
	{ .name = "append", .func = &append },
	{ .name = "last", .func = &last },
	{ .name = "list", .func = &list },
	{ .name = "iterate", .func = &iterate },
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
