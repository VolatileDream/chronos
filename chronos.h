#ifndef __ARCHIVIST_H__
#define __ARCHIVIST_H__

#include <stdint.h>

struct index_header {
	// does this need anything?
};

struct index_key {
	uint32_t seconds; // seconds past epoch
	uint32_t micros; // microseconds past epoch, discounting seconds
};

struct index_entry {
	uint32_t position;
	uint32_t length;
	struct index_key key;
};

struct data_entry {
	uint8_t * value;
};

typedef int (*command_func)( int argc, char** argv );

typedef struct command_t {
	const char* name;
	const command_func func;
} command_t;

int usage( int argc, char** argv);
int init( int argc, char** argv);
int append( int argc, char** argv);
int get( int argc, char** argv);
int list( int argc, char** argv);

// TODO this should be a boolean
int lock_dir( char* dir, int will_write );
int release_dir( int fd );

#define INDEX_FILE "index"
#define DATA_FILE "data"

// TODO will_write should be boolean
// returns an fd, or -1
int open_data( char* dir, int will_write );
// optionally returns the # of entries in the index
int open_index( char* dir, int will_write, int * out_entry_count );

// maximum length of the path
#define PATH_BUFFER_SIZE 1024

int format_key( char * str, int max, struct index_key * key );

#endif /* __ARCHIVIST_H__ */
