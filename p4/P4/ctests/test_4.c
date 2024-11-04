#include "types.h"
#include "stat.h"
#include "user.h"
#include "psched.h"

int getnice(int pid) {
    struct pschedinfo st;
    
    if (getschedstate(&st) == 0) {
        for (int i = 0; i < NPROC; i++) {
            if (st.inuse[i] && st.pid[i] == pid) {
                return st.nice[i];
            }
        }
    } else {
        return -1;
    }
    return -1;
}

int
main(int argc, char *argv[])
{  
    if(nice(21) != -1) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }
  
    if (getnice(getpid()) != 0) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }

    if (nice(-1) != -1) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }
  
    printf(1, "XV6_SCHEDULER\t SUCCESS\n");    
    exit();
}
