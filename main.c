
#include "chronos.h"

// printf
#include <stdio.h>

// exit
#include <stdlib.h>

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

	printf(" iterate <directory> print-format\n");
	printf("   print-format: a string of what to output\n");
	printf("     %%k - outputs the key\n");
	printf("     %%v - outputs the content\n");
	printf("     %%%% - outputs a literal %%\n");
	
	printf("\n");

	printf("%s expects keys to parse to a date\n", argv[0]);
	return 0;
}

static void bad_key( int rc, char * key ){
	fprintf( stderr, "couldn't parse key value: %s\n", key);
	switch(rc){
		case 1:
			fprintf( stderr, "invalid date-time\n" );
			break;
		case 2:
			fprintf( stderr, "invalid nanoseconds\n" );
			break;
	}
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

typedef int (*open_func)( struct chronos_handle * handle, int argc, char** argv );

static int do_open_func( int argc, char** argv, open_func func ){

	char * dir = argv[2];
	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_only, & handle );

	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	rc = (*func)( & handle, argc, argv );

	chronos_close( & handle );

	return rc;

}

static int do_count( struct chronos_handle * handle, int argc, char** argv ){
	int count;
	int rc = chronos_stat( handle, & count, NULL );
	if( rc != 0 ){
		perror("chronos_stat");
		return rc;
	}

	printf("%d\n", count );

	return 0;
}

static int count( int argc, char** argv ){
	return do_open_func( argc, argv, & do_count );
}

static int do_get( struct chronos_handle * handle, int argc, char** argv ){
	struct index_key key;
	int rc = parse_key( argv[3], strlen( argv[3] ), & key );
	if( rc != 0 ){
		bad_key(rc, argv[3]);
		return rc;
	}

	struct index_entry entry;

	rc = chronos_find( handle, & key, & entry );
	if( rc != 0 ){
		if( rc != C_NOT_FOUND ){
			perror("chronos_find");
		}
		return rc;
	}

	// output to stdout
	chronos_output( handle, & entry, 1 );
	if( rc != 0 ){
		perror("chronos_output");
		return rc;
	}

	return 0;
}

static int get( int argc, char** argv ){

	if( argc != 4 ){
		fprintf(stderr, "bad number of arguments to get\n" );
		return -1;
	}

	int rc = do_open_func( argc, argv, & do_get );

	return rc;
}

static int do_append( struct chronos_handle * handle, int argc, char** argv ){
	int key_parsed = 0;
	struct index_key key;

	if( argc > 4 && strncmp("-t", argv[3], 2) == 0 ){
		int rc = parse_key( argv[4], strlen( argv[4] ), & key );
		if( rc == 0 ){
			key_parsed = 1;
		} else {
			bad_key(rc, argv[4]);
			return rc;
		}
	}

	int rc = 0;
	if( key_parsed ){
		rc = chronos_append( handle, & key, 0 );
	} else {
		rc = chronos_append( handle, NULL, 0 );
	}
	if( rc != 0 ){
		if( rc == C_PROVIDED_KEY_NOT_LATEST ){
			fprintf( stderr, "can't insert keys before the last entry.\n");
		} else {
			perror("chronos_append");
		}
		return rc;
	}

	return 0;
}

static int append( int argc, char** argv ){
	return do_open_func( argc, argv, & do_append );
}

static int do_last( struct chronos_handle * handle, int argc, char** argv ){
	struct index_entry entry;

	int rc = chronos_entry( handle, -1, & entry );
	if( rc != 0 ){
		if( rc != C_NO_MORE_ELEMENTS ){
			perror("chronos_entry");
		}
		return rc;
	}

	char buffer[1024];
	format_key( buffer, sizeof(buffer), & entry.key );
	printf( "%s\n", buffer );

	return 0;
}

static int last( int argc, char** argv ){
	return do_open_func( argc, argv, & do_last );
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

	char * out = argv[3];
	size_t len = strlen(out);
	int rc = 0;

	for(int i=0; i < len && rc == 0; i++){
		if( out[i] == '%' ){
			i++; // consume the char, this is an escape code
			switch( out[i] ){
				case '%':
					rc = write( 1, "%", 1 );
					break;
				case 'k':
					rc = write( 1, buffer, length );
					break;
				case 'v':
					chronos_output( handle, entry, 1 );
					break;
				default:
					// this is undefined behaviour
					fprintf( stderr, "bad format string at index %d: %s\n", i, out );
					rc = 1;
					break;
			}
		} else {
			rc = write( 1, out + i, 1 );
		}
	}
	if( rc != 0 ){
		exit(-1);
	}
}
int iterate( int argc, char** argv ){
	if ( argc < 4 ){
		fprintf( stderr, "insufficient arguments, print-format required\n");
		return 1;
	}
	return do_iterate( argc, argv, & full_iter );
}

// commands for the cli interface
typedef int (*command_func)( int argc, char** argv );

typedef struct command_t {
       const char* name;
       const command_func func;
} command_t;

static command_t commands[] = {
	{ .name = "append", .func = &append },
	{ .name = "get", .func = &get },
	{ .name = "last", .func = &last },
	{ .name = "list", .func = &list },
	{ .name = "iterate", .func = &iterate },
	{ .name = "count", .func = &count },
	{ .name = "--help", .func = &usage },
	{ .name = "-h", .func = &usage },
	{ .name = "?", .func = &usage },
	{ .name = "init", .func = &init },
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
