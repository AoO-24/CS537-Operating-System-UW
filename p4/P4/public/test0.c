//Tests opening small number of files

#include "types.h"
#include "user.h"
#include "param.h"

int
main(int argc, char *argv[])
{
  int nextpid = getnextpid();
  
  // fork a child process.
  int childpid = fork();
  if(childpid < 0) {
    printf(1, "TEST FAILED: fork failed");
    exit();
  }

  if(childpid > 0) {
    // check if child was assigned next pid.
    if (childpid == nextpid)
      printf(1, "TEST PASSED\n");
    else
      printf(1, "TEST FAILED: nextpid returned = %d, expected nextpid = %d", nextpid, childpid);

    wait();
  } else {
    sleep(3);
  }

  exit();
}
