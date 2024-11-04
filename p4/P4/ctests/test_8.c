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
        sleep(100);
        int p1ticks1, p2ticks1, p1ticks2, p2ticks2;
        int uptime_begin, uptime_end;
        uptime_begin = uptime();
        p1ticks1 = getticks(p1);
        p2ticks1 = getticks(p2);
        printf(1, "p1 ticks: %d\t p2 ticks: %d\n", p1ticks1, p2ticks1);

        spin();

        p1ticks2 = getticks(p1);
        p2ticks2 = getticks(p2);
        uptime_end = uptime();
        int timediff, p2time;
        timediff = uptime_end - uptime_begin;
        printf(1, "p1 ticks: %d\t p2 ticks: %d\ttimediff: %d\n", p1ticks2, p2ticks2, timediff);

        p2time = p2ticks2 - p2ticks1;
        if (timediff > 100 || timediff - p2time > 1) {
            printf(1, "XV6_SCHEDULER\t FAILED\n");
            exit();
        }

        printf(1, "XV6_SCHEDULER\t SUCCESS\n");
        exit();
    }
}
