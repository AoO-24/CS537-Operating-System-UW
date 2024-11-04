#include "types.h"
#include "stat.h"
#include "user.h"
#include "psched.h"

void spin() {
    int i;
    for (i = 0; i < 100000000; i++) {
        asm("nop");
    }
}

int getpriority(int pid) {
    struct pschedinfo st;
    
    if (getschedstate(&st) == 0) {
        for (int i = 0; i < NPROC; i++) {
            if (st.inuse[i] && st.pid[i] == pid) {
                return st.priority[i];
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
    int pid = getpid();
    if (getpriority(pid) != 0) {
        printf(1, "initial priority was non-zero\n");
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }

    spin();
    sleep(120);

    int priority;
    if ((priority = getpriority(pid)) == 0) {
        printf(1, "priority was %d\n", priority);
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }

    printf(1, "priority of %d is %d", pid, priority);
    printf(1, "XV6_SCHEDULER\t SUCCESS\n");
    exit();
}
