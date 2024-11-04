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

int getticks(int pid) {
    struct pschedinfo st;
    
    if (getschedstate(&st) == 0) {
        for (int i = 0; i < NPROC; i++) {
            if (st.pid[i] == pid && st.inuse[i] == 1) return st.ticks[i];
        }
    }
    return -1;
}

int main(int argc, char *argv[])
{
    int p1, p2;
    p1 = fork();
    if (p1 < 0) {
        printf(1, "p1 fork failed\n");
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    } else if (p1 == 0) { // child p1
        nice(20);
        sleep(100);

        spin();
        exit();
    }

    p2 = fork();
    if (p2 < 0) {
        printf(1, "p2 fork failed\n");
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    } else if (p2 == 0) { // child p2
        sleep(300);

        spin();
        exit();
    }
    
    sleep(300);
    int p1ticks, p2ticks;
    p1ticks = getticks(p1);
    p2ticks = getticks(p2);
    printf(1, "p1 ticks: %d\t p2 ticks: %d\n", p1ticks, p2ticks);

    if (p1ticks <= p2ticks) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }

    printf(1, "XV6_SCHEDULER\t SUCCESS\n");
    exit();
}

