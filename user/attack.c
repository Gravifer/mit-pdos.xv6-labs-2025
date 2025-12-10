#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define DATASIZE (10*4096)
char buf[DATASIZE];

int
main(int argc, char *argv[])
{
  // Your code here.
  // explicitly use uninitialized memory
  int is_string = 0;
  for (int i = 3*4096; i < 4*4096; i++)
  {
    // inspect each byte
    if(buf[i] != '\0') {
      // if (!is_string) {
      //   printf("%s\n", &buf[i]);
      // }
      printf("%d -> 0x%x (%c)\n", i, buf[i], buf[i]);
      is_string = 1;
    } else {
      if (is_string) printf("-----------------\n");
      is_string = 0;
    }
  }
  printf("ans: {\n%s\n}\n", &buf[3*4096 + 16]);
  printf("=================\n");

  for (int i = 9 * 4096; i < DATASIZE; i++)
  {
    // inspect each byte
    if(buf[i] != '\0') {
      is_string = 1;
      printf("%d -> 0x%x (%c)\n", i, buf[i], buf[i]);
    } else {
      if (is_string) printf("-----------------\n");
      is_string = 0;
    }
  }
  // printf("Secret data leak: %s\n", &buf[1]);
  printf("ans: {\n%s\n}\n", &buf[9*4096 + 23]);

  exit(0);
}
