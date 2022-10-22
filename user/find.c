#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  // Return blank-padded name.
  if (strlen(path) >= DIRSIZ) {
    return path;
  }

  memmove(buf, path, strlen(path));
  memset(buf + strlen(path), ' ', DIRSIZ - strlen(path));
  return buf;
}


/**
 * ./ => 1
 * ../ => 1
 * . => 1
 * .. => 1
 */
int isDot(char* path) {
  //printf("is dot: %s (len: %d)\n", path, strlen(path));
  
  int len = strlen(path);
  if ((path[0] == '.' && len == 1)
    || (path[0] == '.' && path[1] == '.' && len == 2)
    || (path[0] == '.' && path[1] == '/' && len == 2)
    || (path[0] == '.' && path[1] != '.' && path[2] == '/' && len == 3)) {
    return 1;
  }
  return 0;
}


void
find(char* src_path, char* file_name)
{
  char buf[512];
  char* p;

  int fd; // src path fd
  if ((fd = open(src_path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", src_path);
    return;
  }

  struct stat st; // to store file status
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", src_path);
    return;
  }

  switch (st.type)
  {
  case T_FILE:
    fprintf(2, "find: src_path need dir, not file (%s)\n", src_path);
    return;
  
  case T_DIR:
    if (strlen(src_path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "find: src_path too long (%s)\n", src_path);
      break;
    }
    strcpy(buf, src_path); // now buf is src_path, then buf'll store fully file name
    p = buf + strlen(buf); // now p is at the end of src_path stored in buf
    *p = '/'; // append '/' to buf
    p++;

    struct dirent de;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) { // iterate files under dir
      if (de.inum == 0) {
        continue;
      }
      memmove(p, de.name, DIRSIZ); // append file name to {src_path/}
      p[DIRSIZ] = 0; // \0
      if (stat(buf, &st)) {
        fprintf(2, "find: cannot stat %s\n", buf);
        continue;
      }

      //printf("de.name: %s and st.type: %d\n", de.name, st.type);
      switch (st.type)
      {
      case T_DIR: // file under src_path is also a dir
        if (isDot(de.name) != 1) {
          find(de.name, file_name);
        }
        break; // at once after having processed dir 
      
      case T_FILE:
        if (strcmp(de.name, file_name) != 0) { // files not satisfied
          continue;
        }
        printf("%s\n", buf);
        break;
      }
    }
    break;
  }
  close(fd);
}


int
main(int argc, char* argv[])
{
  if (argc < 3) {
    fprintf(2, "find: arg num must be larger than 2\n");
    exit(1);
  }

  char* src_path = argv[1];
  char* file_name = argv[2];

  find(src_path, file_name);

  exit(0);
}
