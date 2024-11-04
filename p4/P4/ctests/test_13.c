#include "types.h"
#include "stat.h"
#include "user.h"
#include "psched.h"
#define PROC 4

void spin()
{
    int i = 0;
    int j = 0;
    int k = 0;
    for(i = 0; i < 100; ++i) {
        for(j = 0; j < 10000000; ++j) {
            k = j % 10;
            k = k + 1;
        }
    }
}

void spin2()
{
    int j = 0;
    int k = 0;
        for(j = 0; j < 10000000; ++j) {
            k = j % 10;
            k = k + 1;
        }
}

int
main(int argc, char *argv[])
{
    struct pschedinfo st;
    int count = 0;
    int i = 0;
    int pid[NPROC];
    printf(1,"Spinning...\n");
    while(i < PROC) {
        pid[i] = fork();
        if(pid[i] == 0) {
            int n;
            for (n = 0; n < 100; n++) {
                if (i%2) {
                    spin();
                } else {
                    spin2();
                    sleep(50);
                }
                asm("nop");
            }
            exit();
        }
        i++;
    }
    sleep(500);

    if(getschedstate(&st) != 0) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }

    printf(1, "\n**** PInfo ****\n");
    for(i = 0; i < NPROC; i++) {
        if (st.inuse[i]) {
            count++;
            printf(1, "pid: %d ticks: %d priority: %d\n", st.pid[i], st.ticks[i], st.priority[i]);
        }
    }
    for(i = 0; i < PROC; i++) {
        kill(pid[i]);
    }
    while (wait() > 0);
    printf(1,"Number of processes in use %d\n", count);

    if(count == 7) {
        printf(1, "XV6_SCHEDULER\t SUCCESS\n");
    }
    else {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
    }

    exit();
}
