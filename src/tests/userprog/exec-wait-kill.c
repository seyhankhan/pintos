/* Wait for a process that will be killed for bad behavior. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int pid = exec ("child-bad");
  msg ("exec() = %d", pid);
  msg ("wait() = %d", wait (pid));
}
