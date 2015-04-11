#ifndef __PID_H__
#define __PID_H__

// for pid_t
#include <sys/types.h>

// error during creation of pidfile
#define PID_CREATE 1

// error during the string format + write of the pid file
#define PID_WRITE 2

// error during attempted access of pid file
#define PID_NO_FILE 3

// error reading pid
#define PID_READ 4

// creates a pid  file in the specified location, guarentees that the file has
// not existed before this function was called.
int pid_file_create( char* file_name );

// returns an error code, after it returns the file is guarenteed to have been deleted
int pid_file_cleanup( char* file_name );

// returns an error code, sticks the pid in out_pid
int pid_file_get_pid( char* file_name, pid_t* out_pid );

#endif /* __PID_H__ */
