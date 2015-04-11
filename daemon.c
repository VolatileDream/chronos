
#include "chronos.h"
#include "pid.h"

// errno
#include <errno.h>

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

// sigaction(2)
#include <signal.h>

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

#define get_fifo_name( buffer, len, dir ) \
	get_name( buffer, len, dir, "%s/fifo" )

#define get_pid_name( buffer, len, dir ) \
	get_name( buffer, len, dir, "%s/pid" )

static int get_name( char* buffer, size_t len, char* directory, char * format ){
	return snprintf(buffer, len, format, directory );
}

static int do_stop( int argc, char ** argv ){
	char* dir = argv[2];

	char pid_name[PATH_BUFFER_SIZE];
	int rc = get_pid_name( pid_name, sizeof(pid_name), dir );
	if( rc >= sizeof(pid_name) ){
		return rc;
	}

	pid_t pid = 0;

	rc = pid_file_get_pid( pid_name, &pid );

	switch(rc){
		case PID_NO_FILE:
			fprintf( stderr, "No daemon pid file.");
			return 0;
		case PID_READ:
			fprintf( stderr, "Error reading pid file.");
			return 1;
		case 0:
			rc = kill(pid, 2); // sigint
			break;
	}

	if( rc != 0 ){
		perror("stop daemon - kill");
	}

	return rc;
}

static int copy_fd( int fd_from, int fd_to ){
	// read from stdin
	while( 1 ){

		ssize_t read = splice( fd_from, NULL, fd_to, NULL, 0x0fffffff, 0 );

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
	int rc = get_fifo_name( fifo_name, sizeof(fifo_name), dir );

	// truncated output	
	if( rc >= sizeof(fifo_name) ){
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

// stored globally so that the sigaction handler can unlink the fifo when it
// runs, and therefore cause the program to exit cleanly.
static char fifo_name[ PATH_BUFFER_SIZE ];

static void term_handle( int sig ){
#ifdef DEBUG
	int rc = write( 1, "unlink\n", 7 );
	(void)rc;
#endif /* DEBUG */
	unlink(fifo_name);
}

static void setup_signal_handler(){
	sigset_t blocked_signals;
	sigfillset( &blocked_signals );

	struct sigaction action;
		action.sa_handler = &term_handle;
		action.sa_mask = blocked_signals; // all signals
		action.sa_flags = 0;

	int rc = sigaction( 15, &action, NULL); //sigterm
	rc |= sigaction( 2, &action, NULL); // sigint
	if( rc ){
		perror("setup signal handler");
		exit(1);
	}
}

static int do_daemon( int argc, char** argv ){

	char* dir = argv[2];
	struct chronos_handle handle;

	int rc = chronos_open( dir, cs_read_write, & handle );
	if( rc != 0 ){
		perror("chronos_open");
		return rc;
	}

	// insert the chronos directory that we want, and then the fifo file
	rc = get_fifo_name( fifo_name, sizeof(fifo_name), dir );
	// truncated output	
	if( rc >= sizeof(fifo_name) ){
		return rc;
	}

	char pid_file [ PATH_BUFFER_SIZE ];
	rc = get_pid_name( pid_file, sizeof(pid_file), dir );
	if( rc >= sizeof(pid_file) ){
		return rc;
	}

	// install signal handler
	setup_signal_handler();

	rc = pid_file_create( pid_file );
	if( rc != 0 ){
		perror("daemon pid-file");
		return rc;
	}

	// R+W user, none to others
	rc = mkfifo(fifo_name, 0600 );

	if( rc != 0 ){
		perror("daemon mkfifo");
		return rc;
	}

	while(1){
		int fd = open( fifo_name, O_RDONLY );
		if( fd < 0 ){
			if( errno == ENOENT ){
				// fifo was removed by our signal handler
				// this is acceptable, so we will just exit.
			} else if ( errno == EINTR ) {
				// got interrupted by a signal, in this case
				// we would normally attempt again, but there
				// is only the unlink-fifo signal handler, so
				// we exit instead.
			} else {
				perror("fifo open");
			}
			break;
		}

		rc = chronos_append( &handle, NULL, fd );

		if( rc != 0 ){
			perror("fifo append");
		}

		close(fd);
	}

	rc = pid_file_cleanup(pid_file);

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
	{ .name = "stop", .func = &do_stop },
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
