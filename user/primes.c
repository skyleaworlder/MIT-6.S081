#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define BEG 2
#define END 35

/**
 * pipes: the former sieve or top pipes connecting this sieve (process) 
 * the first number sieve read must be prime
 * 
 * +-----+ "2",3,4,5...          "3",5,7,9,11...
 * |     |=============> |     |================>
 * |     | .           
 * |     | .
 * +-----+
 */
void
sieve(int* pipes)
{
  // the prime of this sieve
  int p;
  int r_pipe = pipes[0];
  int w_pipe = pipes[1];
  close(w_pipe);

  // the first number read by sieve must be prime
  if (read(r_pipe, (int*)(&p), sizeof(int)) == -1) {
    //printf("sieve: read failed\n");
    exit(1);
  }
  printf("prime: %d\n", p);

  int next_pipes[2];
  pipe(next_pipes);
  int nr_pipe = next_pipes[0];
  int nw_pipe = next_pipes[1];

  if (fork() == 0) {
    // child process
    sieve(next_pipes);
  } else {
    // close read in parent process in order to send data to child process
    close(nr_pipe);

    // new number input
    int new_num;

    // real sieve to filter non-prime
    int still_read = 1;
    do
    {
      if ((still_read = read(r_pipe, (int*)(&new_num), sizeof(int))) == -1) {
        //printf("sieve: inner read failed %d\n", still_read);
        exit(1);
      }

      if (new_num % p != 0) {
        if (write(nw_pipe, (int*)(&new_num), sizeof(int)) == -1) {
          //printf("sieve: inner write failed\n");
          exit(1);
        }
      }
    } while (still_read);
    //close(nw_pipe);
    close(r_pipe);

    int sig;
    wait(&sig);
    //printf("wait end: %d\n", sig);
  }
  //printf("normal exit\n");
  exit(0);
}

int
main(int argc, char* argv[])
{
  int top_pipes[2];
  pipe(top_pipes);
  int r_pipe = top_pipes[0];
  int w_pipe = top_pipes[1];

  if (fork() == 0) {
    // child process
    sieve(top_pipes);
  } else {
    // main process should write data, close read port
    // so that child processes can read data
    close(r_pipe);
    // parent process
    for (int i = BEG; i <= END; i++) {
      int n;
      //printf("push number %d\n", i);
      if ((n = write(w_pipe, (int*)(&i), sizeof(int))) == -1) {
        // fail to write
        //printf("main: write failed\n");
        exit(1);
      }
    }
    close(w_pipe);

    int sig;
    wait(&sig);
    //printf("wait end: %d\n", sig);
  }
  exit(0);
}
