#ifndef _PSCHED_H_
#define _PSCHED_H_

#include "param.h"

struct pschedinfo
{
  int inuse[NPROC];    // whether this slot of the process table is in use (1 or 0)
  int priority[NPROC]; // the priority of each process
  int nice[NPROC];     // the nice value of each process
  int pid[NPROC];      // the PID of each process
  int ticks[NPROC];    // the number of ticks each process has accumulated
};

#endif // _PSCHED_H_
