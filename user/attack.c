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
  // for(int i = 9*4096; i < DATASIZE; i++) {
  //   // inspect each byte
  //   if(buf[i] != 0) {
  //     printf("%d -> 0x%x (%c)\n", i, buf[i], buf[i]);
  //   }
  // }
  // printf("Secret data leak: %s\n", &buf[1]);
  printf("%s\n", &buf[36887]);

  exit(0);
}
