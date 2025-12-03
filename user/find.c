#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  buf[sizeof(buf)-1] = '\0';
  return buf;
}

void find(char* path, char* name, int exec_flag, char* argv[], int argc) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    fprintf(2, "find: %s is not a path.\n", path);
    close(fd);
    return;
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
    printf("find: path too long\n");
  }

    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // printf("DEBUG: buf: %s\n", buf);
    
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0 || !strcmp(de.name,".") || !strcmp(de.name,".."))
        continue;

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

    //   printf("DEBUG: buf/filename: %s\n", buf);

      if (!strcmp(de.name, name)) {
        printf("%s\n", buf);
        if (exec_flag) {
            if(fork() == 0) {
                char* argv_new[MAXARG];
                for (int i = 0; i < argc; i ++) {
                    argv_new[i] = argv[i];
                }
                argv_new[argc] = buf;
                exec(argv[0], argv_new);
            }
            wait(0);
        }
      }
      
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      if (st.type == T_DIR) {
        find(buf, name, exec_flag, argv, argc);
      }
    }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int exec_flag = 0;

  if(argc < 3){
    fprintf(2, "usage: find [directory] [name]\n");
    exit(1);
  }
  if (!strcmp(argv[3], "-exec")) {
    exec_flag = 1;
  }
  find(argv[1], argv[2], exec_flag, argv + 4, argc - 4);
  exit(0);
}