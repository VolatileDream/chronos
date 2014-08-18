#ifndef __CHRONOS_H__
#define __CHRONOS_H__

enum chronos_flags {
	cs_read_only	= 0x1,
	cs_read_write	= 0x2,
	cs_create	= 0x4, // create the 
};

struct chronos_handle {
	enum chronos_flags state; // will only be one of cs_read_*
	int dir_fd;
	int index_fd;
	int data_fd;
};

// Opens (and possibly creates) a chronos event log.
//  if the event log is created, it will also create the entire directory tree if required.
//
//  chronos_state:
//	must include one of cs_read_*
//	cs_create does nothing if the log already exists
//
//  out_handle:
//	TODO allow this to possibly be NULL (ie, create + unlock)?
//
// Returns:
//	* 0 on success
//	* 1 if one of cs_read_* isn't specified, or more than one was specified
//	* 2 if the log doesn't exist, and cs_create wasn't specified
//	* 3 if the directory lookup failed, because of bad permissions, some part of the path wasn't a directory, etc.
//	* 4 if attempting to create the directory failed
//	* 5 if locking the event log failed
int chronos_open( const char * dir, enum chronos_flags flags, struct chronos_handle * out_handle );

// Closes a chronos event log.
//
//  Note that this function attempts to leave errno intact, but if multiple errors
//   occur while attempting to close the log, then only the last will be reported.
//
//  Note that the chronos event log _will_ be closed after this call, regardless of
//   it's return code.
//
// Returns:
//	* 0 on success
//	* -1 on error, errno will be set to the last error that occured.
int chronos_close( struct chronos_handle * handle );

#include <stdint.h>

// min macro from: https://stackoverflow.com/questions/3437404/min-and-max-in-c
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

struct index_key {
	uint32_t seconds; // seconds past epoch
	uint32_t nanos; // nanoseconds past epoch, discounting seconds
};

struct index_entry {
	uint32_t position;
	uint32_t length;
	struct index_key key;
};

// Outputs the given entry out to the specified file descriptor.
//
// Returns:
//	* 0 on success
int chronos_output( struct chronos_handle * handle, struct index_entry * entry, int fd_out );

/*
// Searches the chronos event log for the specified key
// Returns:
//	* 0 if an entry exists for the key, and fills out_entry with the entry.
int chronos_find( struct chronos_handle * handle, struct index_key * search_key, struct index_entry * out_entry );

int chronos_list( struct chronos_handle * handle, ... );
*/

// Attempts to format the key in the buffer.
//
// Returns:
//	* the number of bytes taken up by the formatted key
//	* 0 if formatting the key ran out of space
int format_key( char * str, int max, struct index_key * key );
// Attempts to parse the key in the buffer.
//
// Returns:
//	* 0 on success
//	* 1 if the date-time is invalid
//	* 2 if the nanoseconds are invalid
int parse_key( char * str, int length, struct index_key * out_key );

#endif /* __CHRONOS_H__ */
