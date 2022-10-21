#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{
  // pipefd[0] refers to the read end of the pipe.
  // pipefd[1] refers to the write end of the pipe.
  //
  // parent_pipes for parent 0 -> read pipe; 1 -> write pipe
  // child_pipes for child 0 -> read pipe; 1 -> write pipe 
  int parent_pipes[2];
  int child_pipes[2];
  if (pipe(parent_pipes) < 0) {
    printf("pingpong: pipe(fds) for parent_pipes call error\n");
    exit(1);
  }
  if (pipe(child_pipes) < 0) {
    printf("pingpong: pipe(fds) for child_pipes call error\n");
    exit(1);
  }

  // child process id
  int pid = fork();
  if (pid == 0) {
    // child process read then write
    int n;
    char r_buf[50];
    close(parent_pipes[1]);
    close(child_pipes[0]);
    if ((n = read(parent_pipes[0], &r_buf, sizeof(r_buf))) == -1) {
      printf("pingpong: child read() error: %d\n", n);
      exit(1);
    }

    char w_buf[50] = "???";
    if ((n = write(child_pipes[1], w_buf, sizeof(w_buf))) == -1) {
      printf("pingpong: child write() error: %d\n", n);
      exit(1);
    }

    int current_pid = getpid();
    printf("%d: received ping: %s\n", current_pid, r_buf);
    exit(0);
  } else {
    // parent process
    char r_buf[50];
    char w_buf[50] = "!!!";
    int n;
    close(parent_pipes[0]);
    close(child_pipes[1]);
    if ((n = write(parent_pipes[1], w_buf, sizeof(w_buf))) == -1) {
      printf("pingpong: parent write() error: %d\n", n);
      exit(1);
    }

    // wait for child process exit here
    int child_sig;
    wait(&child_sig);
    if (child_sig != 0) {
      printf("pingpong: child exit unexpected: %d\n", child_sig);
      exit(1);
    }

    if ((n = read(child_pipes[0], r_buf, sizeof(r_buf))) == 1) {
      printf("pingpong: parent read() error: %d\n", n);
      exit(1);
    }

    int current_pid = getpid();
    printf("%d: received pong: %s\n", current_pid, r_buf);
  }
  exit(0);
}
