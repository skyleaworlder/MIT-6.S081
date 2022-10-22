#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

void
copy(char** dst, char** src, int num)
{
  for (int i = 0; i < num; i++) {
    char* d = *(dst + i);
    char* s = *(src + i);

    d = malloc(20);
    strcpy(d, s);
  }
}

/**
 * argc must > 0 
 */
void
xargs(int argc, char* argv[])
{
  char ch;
  char* pass_to_cmd[MAXARG];

  int n; // read num
  int arg_num = 0;
  int pos = 0;

  // pass original arguments
  for (int i = 0; i < argc; i++) {
    pass_to_cmd[i] = argv[i];
    arg_num++;
  }

  pass_to_cmd[arg_num] = malloc(20);
  while ((n = read(0, &ch, 1)) == 1) {
    if (ch == '\n' || pos >= 13) {
      pass_to_cmd[arg_num][pos] = '\0';
      // allocate new memory
      arg_num++;
      pos = 0;
      pass_to_cmd[arg_num] = malloc(20);
    } else {
      pass_to_cmd[arg_num][pos] = ch;
      pos++;
    }
  }

  char* cmd = argv[0];
  if (fork() == 0) {
    // child process
    exec(cmd, pass_to_cmd);
    exit(0);
  } else {
    // parent process
    int sig;
    wait(&sig);
    exit(0);
  }
}

int
main(int argc, char* argv[])
{
  if (argc <= 1) {
    fprintf(2, "xargs: must have an arg\n");
    exit(1);
  }
  if (argc >= 2) {
    xargs(argc - 1, argv + 1);
  }
  exit(0);
}
