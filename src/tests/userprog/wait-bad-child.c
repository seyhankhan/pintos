/* Wait for a subprocess to finish. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int pid = wait (exec ("exec-exit"));
  msg ("grandchild should be running by now")
  msg ("waiting for grandchild returned %d", wait(pid));
}
