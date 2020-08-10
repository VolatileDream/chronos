
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char* BAD_ARGS = "need more args\n";
static char* WRITE = "abc";

int main(int argc, char** argv) {
  char ** write_arg = &WRITE;
  if (argc < 2) {
    write(2, BAD_ARGS, strlen(BAD_ARGS));
    return -1;
  } else if (argc >= 3) {
    write_arg = &argv[2];
  }

  signal(SIGPIPE, SIG_IGN);

  int write_len = strlen(*write_arg);
  while(1) {
    int fd = open(argv[1], O_WRONLY);
    if (fd < 0) {
      perror("open dir");
      return 1;
    }
    int rc = write(fd, *write_arg, write_len);
    close(fd);
    if (rc == -1 && errno == EPIPE) {
      // Ignore EPIPE until file open fails.
      // At that point the daemon exited, so we should too.
      continue;
    } if (rc != 3) {
      perror("write");
      return 2;
    }
  }
  return 0;
}
