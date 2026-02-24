//
// formatted console output -- printf, panic.
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

volatile int panicking = 0; // printing a panic message
volatile int panicked = 0; // spinning forever at end of a panic

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
} pr;

static char digits[] = "0123456789abcdef";

static void
printint(long long xx, int base, int sign)
{
  char buf[20];
  int i;
  unsigned long long x;

  if(sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}

static void
printptr(uint64 x)
{
  int i;
  consputc('0');
  consputc('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    consputc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console.
int
printf(char *fmt, ...)
{
  va_list ap;
  int i, cx, c0, c1, c2;
  char *s;

  if(panicking == 0)
    acquire(&pr.lock);

  va_start(ap, fmt);
  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      consputc(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0) c1 = fmt[i+1] & 0xff;
    if(c1) c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      printptr(va_arg(ap, uint64));
    } else if(c0 == 'c'){
      consputc(va_arg(ap, uint));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
    } else if(c0 == '%'){
      consputc('%');
    } else if(c0 == 0){
      break;
    } else {
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c0);
    }

  }
  va_end(ap);

  if(panicking == 0)
    release(&pr.lock);

  return 0;
}

void
panic(char *s)
{
  panicking = 1;
  printf("panic: ");
  printf("%s\n", s);
  panicked = 1; // freeze uart output from other CPUs
  backtrace();
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
}

/* a backtrace to be used in SYS_PAUSE
  output should be a list of return addresses with this form (but the numbers will likely be different):
    backtrace:
    0x0000000080002cda
    0x0000000080002bb6
    0x0000000080002898
*/

// Weak stub; will be overridden by symbols.c
__attribute__((weak)) const char*
address_to_symbol(uint64 addr)
{
  return 0;
}

void
backtrace(void)
{
  uint64 fp = r_fp();
  uint64 stack_page = PGROUNDDOWN(fp);
  printf("backtrace:\n");
  while(fp && PGROUNDDOWN(fp) == stack_page){
    uint64 ra = *((uint64*)fp - 1); // return address is at fp - 8
    const char *sym = address_to_symbol(ra);
    printf("%p %s\n", (void *)ra, sym ? sym : "?");
    fp = *((uint64*)fp - 2); // previous frame pointer is at fp - 16
  }

  if (0) { // temporary diagnostics: dump each frame's nearby words for manual inspection.
    fp = r_fp();
    printf("backtrace frame dump:\n");
    while(fp && PGROUNDDOWN(fp) == stack_page){
      uint64 prev_fp = *((uint64*)fp - 2);
      uint64 ra = *((uint64*)fp - 1);
      printf("  frame fp=%p page=%p\n", (void*)fp, (void*)PGROUNDDOWN(fp));
      printf("    [fp-16] saved fp : %p\n", (void*)prev_fp);
      printf("    [fp-8 ] saved ra : %p (callsite %p)\n", (void*)ra, (void*)(ra - 4));
      printf("    [fp+0 ] word     : %p\n", (void*)(*(uint64*)(fp + 0)));
      printf("    [fp+8 ] word     : %p\n", (void*)(*(uint64*)(fp + 8)));
      printf("    [fp+16] word     : %p\n", (void*)(*(uint64*)(fp + 16)));
      fp = prev_fp;
    }
  }
}
