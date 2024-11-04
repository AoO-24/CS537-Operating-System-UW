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
            if (st.pid[i] == pid && st.inuse[i] == 1) return st.priority[i];
        }
    }
    return -1;
}

int main(int argc, char *argv[])
{
    int p1, p2;
    p2 = getpid();
    p1 = fork();
    if (p1 < 0) {
        printf(1, "p1 fork failed\n");
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    } else if (p1 == 0) { // child p1
        int i;
        for (i = 0; i < 10; i++) {
            spin();
            asm("nop");
        }
        exit();
    } else {
        sleep(200);
        int p1priority, p2priority;
        p1priority = getpriority(p1);
        p2priority = getpriority(p2);
        printf(1, "p1 priority: %d\t p2 priority: %d\n", p1priority, p2priority);

        wait();
        if (p1priority <= p2priority) {
            printf(1, "XV6_SCHEDULER\t FAILED\n");
            exit();
        }
        printf(1, "XV6_SCHEDULER\t SUCCESS\n");
        exit();
    }
}
