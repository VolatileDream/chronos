#include "chronos.h"
#include "chronos-internal.h"

// errno
#include <errno.h>

// snprintf
#include <stdio.h>

// mkdir + stat
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// open
#include <fcntl.h>

// flock
#include <sys/file.h>

// strdup
#include <string.h>

// free
#include <stdlib.h>

// sendfile
#include <sys/sendfile.h>

// min macro from: https://stackoverflow.com/questions/3437404/min-and-max-in-c
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#define INVALID_FD (-1)

#define FILE_MODE (S_IRUSR | S_IWUSR)
#define DIRECTORY_MODE ( FILE_MODE | S_IXUSR )

static int mkdir_r( const char * dir ){
	// this is also a beautifully simple solution. mostly taken from:
	// https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix

	char buffer[ PATH_BUFFER_SIZE ];

	// use the returned size
	int size = snprintf( buffer, sizeof(buffer), "%s", dir );

	if( size > PATH_BUFFER_SIZE ){
		// problem.
		errno = ENAMETOOLONG;
		return C_CREATE_FAILED;
	}

	// we should be able to handle a string like "/abc/def//"
	for( int i=1; i < size; i++){
		if( buffer[ size - i ] == '/' ){
			// remove the trailing slash
			buffer[ size - i ] = 0;
		} else {
			break;
		}
	}

	// we can't start with p = buffer, because that would break
	// in the case that buffer is an absolute path. and while this
	// is a little odd for the relative case ( "./" or "a/" ), it
	// has to handle relative directories as well.
	for( char * p = buffer + 1 ; *p ; p++ ){
		if( *p == '/' ){
			*p = 0;
			int rc = mkdir( buffer, DIRECTORY_MODE );
			if( rc != 0 && errno != EEXIST ){
				return C_CREATE_FAILED;
			}
			*p = '/';
		}
	}
	int rc = mkdir( buffer, DIRECTORY_MODE ); // the full thing, since we haven't already
	if( rc != 0 && errno != EEXIST ){
		return C_CREATE_FAILED;
	}

	struct stat dir_stat;

	// stat-ing failed, or it's not a directory
	if( stat( dir, &dir_stat ) != 0 || ! S_ISDIR(dir_stat.st_mode) ){
		return C_CREATE_FAILED;
	}

	return 0;
}

static int require_directory( const char * dir, enum chronos_flags flags, int * out_dir_fd ){

	// attempt to stat the directory
	struct stat stat_buffer;
	int dir_stat_rc = stat( dir, &stat_buffer );

	if( dir_stat_rc == -1 ){
		// check the list of conditions we can deal with

		// 1+ path components doesn't exist
		if( errno == ENOENT ){
			if( flags & cs_create ){
				int mkdir_rc = mkdir_r( dir );
				if( mkdir_rc != 0 ){
					// creating failed.
					return mkdir_rc;
				}
			} else {
				// doesn't exist, but we weren't told to create
				return C_NO_LOG_NO_CREATE;
			}
		} else {
			// lookup failed, can't handle it.
			return C_LOOKUP_FAILED;
		}
	}

	// at this point the path exists.
	dir_stat_rc = stat( dir, &stat_buffer );

	if( dir_stat_rc == -1 || ! S_ISDIR( stat_buffer.st_mode ) ){
		// event log directory itself wasn't a directory.
		return C_LOOKUP_FAILED;
	}

	int fd = open( dir, O_RDONLY );

	if( fd == -1 ){
		// attempting to open the directory failed.
		return C_LOOKUP_FAILED;
	}

	*out_dir_fd = fd;

	return 0;
}

static int get_dir_lock( struct chronos_handle * handle ){
	int lock_flags = LOCK_SH;
	if( handle->state & cs_read_write ){
		lock_flags = LOCK_EX;
	} else {
		// for read only we don't need the lock.
		// this is because of the way that chronos handles writing
		// to the index and data file. See chronos-physical.c
		return 0;
	}
	if( flock( handle->dir_fd, lock_flags ) != 0 ){
		return C_LOCK_FAILED;
	}

	return 0;
}

int require_open_file( struct chronos_handle * handle, enum chronos_file file, enum chronos_flags flag ){

	int * fd_ptr = NULL;
	char * file_name = NULL;

	if( file == cf_index ){
		fd_ptr = & handle->index_fd;
		file_name = "index";
	} else if( file == cf_data_store ){
		fd_ptr = & handle->data_fd;
		file_name = "data";
	} else {
		return C_BAD_READ_WRITE_ARG;
	}

	int rc;

	if( handle->state != cs_read_write && flag == cs_read_write ){
		// update our lock. now it needs to be exclusive.
		handle->state = cs_read_write;
		rc = get_dir_lock( handle );
		if( rc != 0 ){
			// we didn't get the write lock,
			// so switch the lock type back.
			handle->state = cs_read_only;
			return rc;
		}
	}

	// now we own the correct lock on the file

	if( *fd_ptr == INVALID_FD ){
		// this is unlikely if we had to update the lock.
		
		// open it!
		char buffer[ PATH_BUFFER_SIZE ];
		int size = snprintf(buffer, sizeof(buffer), "%s/%s", handle->directory, file_name );

		if( size > PATH_BUFFER_SIZE ){
			errno = ENAMETOOLONG;
			return C_FILE_OPEN_FAILED;
		}

		// always open files for writing, because we might change our mind about only wanting
		// to read from them later. This won't cause problems with writing to the file, because
		// we use a flock on the directory.

		// create the file if it doesn't already exist. we don't specify O_APPEND because that
		// causes problems with the more efficient splice(2) syscall (used to write data to
		// the data file).
		int fd = open( buffer, O_RDWR | O_CREAT, FILE_MODE );

		if( fd == -1 ){
			return C_FILE_OPEN_FAILED;
		}

		*fd_ptr = fd;
	}

	return 0;
}

int chronos_open( const char * dir, enum chronos_flags flags, struct chronos_handle * out_handle ){

	// check that the read write flags are set, and that they are a power of 2,
	// that is, that only 1 of them is set
	enum chronos_flags read_write_flag = flags & (cs_read_only | cs_read_write);

	if( !read_write_flag || (read_write_flag & (read_write_flag-1)) != 0 ){
		return C_BAD_READ_WRITE_ARG;
	}

	int dir_fd = INVALID_FD;
	int rc = require_directory( dir, flags, & dir_fd );

	if( rc != 0 ){
		return rc;
	}

	// at this point the directory exists, and we have a fd to it
	struct chronos_handle local_handle = {
		.directory = NULL,
		.state = read_write_flag,
		.dir_fd = dir_fd,
		.index_fd = INVALID_FD,
		.data_fd = INVALID_FD,
    .cached_count = -1,
    .cached_data_len = -1,
	};

	rc = get_dir_lock( & local_handle );
	if( rc != 0 ){
		close( dir_fd );
		return rc;
	}

	local_handle.directory = strdup( dir );
	// this should happen sufficiently infrequently that 
	if( local_handle.directory == NULL ){
		flock( dir_fd, LOCK_UN );
		close( dir_fd );
		return C_MEMORY_ALLOC_FAILED;
	}

	*out_handle = local_handle;

	if( read_write_flag == cs_read_write ){
		// special case, fast path write access things.
		// since this hasn't been requested yet, just hope it works.
		require_open_file( out_handle, cf_data_store, cs_read_write );
		require_open_file( out_handle, cf_index, cs_read_write );
	}

	return 0;
}

int chronos_close( struct chronos_handle * handle ){

	int rc = 0;
	int saved_error = 0;

	if( handle->data_fd != INVALID_FD ){
		rc = close(handle->data_fd);
		if( rc != 0 ){
			saved_error = errno;
		}
		handle->data_fd = INVALID_FD;
	}

	if( handle->index_fd != INVALID_FD ){
		rc = close(handle->index_fd);
		if( rc != 0 ){
			saved_error = errno;
		}
		handle->index_fd = INVALID_FD;
	}

	if( handle->dir_fd != INVALID_FD ){
		rc = flock( handle->dir_fd, LOCK_UN );
		if( rc != 0 ){
			saved_error = errno;
		}
		rc = close(handle->dir_fd);
		if( rc != 0 ){
			saved_error = errno;
		}
		handle->dir_fd = INVALID_FD;
	}

	if( handle->directory != NULL ){
		free( handle->directory );
	}

	handle->state = 0;

	// "restore" the last error we saw
	if( saved_error != 0 ){
		errno = saved_error;
		return -1;
	}

	return 0;
}

int chronos_output( struct chronos_handle * handle, struct index_entry * entry, int fd_out ){

	int rc = require_open_file( handle, cf_data_store, cs_read_only );
	if( rc != 0 ){
		return rc;
	}

	off_t offset = entry->position;
	size_t length = entry->length;

	while( length > 0 ){
		ssize_t written = sendfile( fd_out, handle->data_fd, &offset, entry->length );
		if( written < 0 ){
			return 1;
		}
		offset += written;
		length -= written;
	}

	return 0;
}

int chronos_stat( struct chronos_handle * handle, int32_t * out_entry_count, int32_t * out_data_size ){

	// neither of these fstat commands should fail,
	// not if the accompanying open worked. But we assume
	// that they might, and handle it.
	if( out_entry_count != NULL ){

		int rc = require_open_file( handle, cf_index, cs_read_only );
		if( rc != 0 ){
			return rc;
		}

		struct stat stat_buf;

		rc = fstat( handle->index_fd, & stat_buf );
		if( rc != 0 ){
			return C_STAT_ERROR;
		}
    handle->cached_count = stat_buf.st_size / sizeof(struct index_entry);
		*out_entry_count = handle->cached_count;
	}

	if( out_data_size != NULL ){

		int rc = require_open_file( handle, cf_data_store, cs_read_only );
		if( rc != 0 ){
			return rc;
		}

		struct stat stat_buf;

		rc = fstat( handle->data_fd, & stat_buf );
		if( rc != 0 ){
			return C_STAT_ERROR;
		}

    handle->cached_data_len = stat_buf.st_size;
		*out_data_size = handle->cached_data_len;
	}

	return 0;
}

#include <time.h>

int init_key( struct index_key * out_key ){
	struct timespec time;

	int rc = clock_gettime( CLOCK_REALTIME, &time );

	if( rc == -1 ){
		return C_KEY_INIT_FAILED;
	}

	out_key->seconds = time.tv_sec;
	out_key->nanos = time.tv_nsec;

	return 0;
}

// technically the FORMAT bit for STRFTIME, we also add nanos on the end of it.
#define KEY_FORMAT "%Y-%m-%d %H:%M:%S"

int format_key( char * buf, int max, struct index_key * key ){

	time_t seconds = key->seconds;

	struct tm * time = gmtime( & seconds );

	int strf_count = strftime( buf, max, KEY_FORMAT, time );

	if( strf_count == 0 ){
		// not enough space, can't do anything about that, need a larger buffer
		return 0;
	}

	// append nanoseconds to it
	int sn_count = snprintf( buf + strf_count, max - strf_count, ".%09u", key->nanos );

	if( sn_count == 0 ){
		// couldn't add the micros to the string
		return 0;
	}

	return strf_count + sn_count;
}

// parses the string passed as a fraction of a second in nanoseconds.
// ex:
//    "0.3" -> 300000000ns
// returns
//  error: non-zero, undefined value in out_value
//  sucess: zero, nanoseconds parsed go into out_value
int parse_nanoseconds( char * str, int length, uint32_t * out_value ){

	uint32_t out = 0;

	int index=0;
	for(; index < length; index++){
		if( '0' <= str[index] && str[index] <= '9' ){
			out = out * 10 + (str[index] - '0');
		} else {
			break;
		}
	}

	if( index != length ){
		// unable to correctly parse string
		return 2;
	}

	// push the second fraction to be a correct fraction, since we assume
	// that nanoseconds are the largest value this corrects the string:
	// "0.3" and pretends we got "0.300000000" instead.
	for(;index < 9; index++){
		out *= 10;
	}

	*out_value = out;

	// check that the nanoseconds are within the expected range.
	// if the value is too big, it's incorrect.
	if( 0 <= out && out <= 999999999 ){
		return 0;
	} else {
		return 1;
	}
}

#include <stdlib.h>

int parse_key( char * str, int length, struct index_key * out_key ){

	struct tm time;

	char * end_of_date = strptime( str, KEY_FORMAT, &time );

	if( end_of_date == NULL ){
		// this is a failure to parse the string
		return 1;
	}

	// convert to number of seconds since epoch
	out_key->seconds = timegm( &time );

	// +1 because the start of the string will be a '.'
	if( parse_nanoseconds( end_of_date + 1, strlen(end_of_date+1), & out_key->nanos ) ){
		return 2;
	}

	return 0;
}

int index_key_cmp( struct index_key * key1, struct index_key * key2 ){
	if( key1->seconds < key2->seconds ){
		return -1;
	}
	if( key1->seconds > key2->seconds ){
		return 1;
	}
	// seconds are equal, compare nanos
	if( key1->nanos < key2->nanos ){
		return -1;
	}
	if( key1->nanos > key2->nanos ){
		return 1;
	}
	return 0;
}

