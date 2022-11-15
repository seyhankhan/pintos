/* Fakes a system call with a bogus system call number at the very top of the
   stack. The process must be terminated with -1 exit code as the system call
   is invalid. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  /* Set up dummy interrupt frame for the syscall. */
  int p[2];
  p[0] = 42;
  p[1] = 42;

  /* Invoke the system call. */
  asm volatile ("movl %0, %%esp; int $0x30" : : "g" (&p));
  fail ("should have called exit(-1)");
}
