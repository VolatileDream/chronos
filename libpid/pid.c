#include "pid.h"

// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// getpid, pid_t, unlink, write
#include <unistd.h>

// errno
#include <errno.h>

// snprintf
#include <stdio.h>

// this should be large enough for any pid. logic:
// pid_t stores things with 8 bits per word (32 numbers per word) we
// multiply by four to drop drop that ratio down to 8 numbers per
// "word". this way we can ensure that the buffer will always be
// large enough regardless of the size of pid_t on any platform.
// (we use 4 because we want to print pids in decimal)
#define PID_SIZE (sizeof(pid_t)*4)

int pid_file_create( char* file_name ){

	int ret = 0;

	// create the file, only if it doesn't exist, and have it be
	// read +writeable by the current user.
	int fd = open( file_name, O_CREAT | O_EXCL | O_WRONLY, 0600 );

	if( fd < 0 ){
		return PID_CREATE;
	}

	// +1 for the newline + null character at the end.
	char pid_buffer[ PID_SIZE + 2 ];

	pid_t pid = getpid();

	ssize_t pid_size = snprintf( pid_buffer, sizeof(pid_buffer), "%d\n", pid );

	if( pid_size >= (ssize_t)sizeof(pid_buffer) ){
		// something was wrong with our buffer size allocation logic.
		// this was a compile time error. :(
		ret = PID_WRITE;
		goto err;
	}

	// pid_buffer will be small enough to get written out in a single write call.
	int rc = write(fd, pid_buffer, pid_size);

	if( rc <= 0 ){
		// ???
		ret = PID_WRITE;
		goto err;
	}

	close(fd);

	return 0;

err:;

	if( fd >= 0 ){
		// save + restore because we invoke calls that will change it.
		int old_errno = errno;

		close(fd);
		unlink(file_name); // unlink because we failed to setup

		errno = old_errno;
	}

	return ret;
}

int pid_file_cleanup( char* file_name ){

	// kind of hacky to open the file rather than use fstat to check if it
	// exists, but this follows the pattern of other functions in this file.
	int fd = open( file_name, O_RDONLY );
	if( fd < 0 ){
		return PID_NO_FILE;
	}
	close(fd);
	unlink(file_name);

	return 0;
}

int pid_file_get_pid( char* file_name, int* out_pid ){

	int ret = 0;

	int fd = open( file_name, O_RDONLY );
	if( fd < 0 ){
		ret = PID_NO_FILE;
		goto err;
	}

	char pid_buffer[ PID_SIZE + 1 ];

	ssize_t length = read( fd, pid_buffer, sizeof(pid_buffer) );

	// didn't read anything, an error reading, or read too much.
	// we shouldn't read too much, because we padded the buffer to have a
	// null byte at the end of it. :/
	if( length <= 0 || length == sizeof(pid_buffer) ){
		ret = PID_READ;
		goto err;
	}

	pid_buffer[ length ] = 0; //stick the null byte on the end

	int rc = sscanf(pid_buffer, "%d\n", out_pid );

	if( rc != 1 ){
		ret = PID_READ;
		goto err;
	}

err:;
	if( fd > 0 ){
		close(fd);
	}

	return ret;
}
