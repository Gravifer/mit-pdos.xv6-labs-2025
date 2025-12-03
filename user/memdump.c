#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data)
{
  // Your code here.
  for (int i = 0; i < sizeof(fmt); i ++) {
    switch (fmt[i])
    {
    case 'i': {
      uint32 prt = 0;
      for (int j = 0; j < 4; j ++) {
        prt |= (((uint32)(*(data + j))) << (j * 8));
      }
      printf("%d\n", prt);
      data += 4;
    }
      break;
    case 'p': {
      uint64 prt = 0;
      for (int j = 0; j < 8; j ++) {
        prt |= (((uint64)(*(data + j))) << (j * 8));
      }
      printf("%lx\n", prt);
      data += 8;
    }
      break;
    case 'h': {
      uint16 prt = 0;
      for (int j = 0; j < 2; j ++) {
        prt |= (((uint16)(*(data + j))) << (j * 8));
      }
      printf("%d\n", prt);
      data += 2;
    }
      break;
    case 'c': {
      char prt = *data;
      printf("%c\n", prt);
      data += 1;
    }
      break;
    case 's': {
      uint64 prt = 0;
      for (int j = 0; j < 8; j ++) {
        prt |= (((uint64)(*(data + j))) << (j * 8));
      }
      printf("%s\n", (char *)prt);
      data += 8;
    }
      break;
    case 'S': {
      printf("%s\n", data);
    }
      break;
    default:
      break;
    }
  }
}
