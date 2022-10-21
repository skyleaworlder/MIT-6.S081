#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc <= 1) {
    printf("sleep: should pass in a number as sleep time.\n");
    exit(1);
  }

  char* time_str = argv[1];
  int time;

  if ((time = atoi(time_str)) <= 0) {
    printf("sleep: unexpected input, input 0 or cannot be transformd to int.\n");
    exit(1);
  }

  sleep(time);
  exit(0);
}
