#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char sep[] = "-\r\t\n./,";

void 
wrap_write(char* num_str, int j) {
    int i = 0;
    // char s1 = '^';
    char s2 = '\n';
    for (i = 0; i < j; i ++) {
        if (num_str[i] != '0')
            break; 
    }
    // write(1, &s1, 1);
    write(1, &num_str[i], j-i);
    write(1, &s2, 1);
}

int
is_sep(char c) {
    for (int i = 0; i < sizeof(sep); i ++) {
        if (c == sep[i]) {
            return 1;
        }
    }
    return 0;
}

int
is_num(char c) {
    return c >= '0' && c <= '9';
}

void
sixfive(int fd) {
    char buffer[256];
    char number[256];
    int n;
    while((n = read(fd, buffer, sizeof(buffer))) > 0) {
        int j = 0;
        for (int i = 0; i < n; i ++) {
            if (is_num(buffer[i])) {
                number[j] = buffer[i];
                j++;
                if (is_sep(buffer[i+1])) {
                    int num = atoi(number);
                    if ((num % 5 == 0 || num % 6 == 0) && num != 0) {
                        wrap_write(number, j);
                    }
                    for (int k = 0; k < j; k ++) {
                        number[k] = ' ';
                    }
                    j = 0;
                    i = i + 1;
                }
            }
        }
    }
    if(n < 0){
        fprintf(2, "cat: read error\n");
        exit(1);
    } 
}

int
main (int argc, char *argv[]) {
    int fd;
    if (argc <=1) {
        fprintf(2, "usage: sixfile [filename.txt]\n");
        exit(1);
    }
    for (int i = 1; i < argc; i ++) {
        if((fd = open(argv[i], O_RDONLY)) < 0){
        fprintf(2, "cat: cannot open %s\n", argv[i]);
        exit(1);
        }
        sixfive(fd);
        close(fd);
    }
    exit(0);
}