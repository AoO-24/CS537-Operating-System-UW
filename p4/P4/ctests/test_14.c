#include "types.h"
#include "stat.h"
#include "user.h"
#include "psched.h"

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
    int childpid = fork();
    if (childpid == 0) {
        nice(20);
        sleep(200);
        exit();
    }
        
    sleep(150);

    if (getpriority(pid) >= getpriority(childpid)) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }
    
    printf(1, "XV6_SCHEDULER\t SUCCESS\n");
    exit();
}
