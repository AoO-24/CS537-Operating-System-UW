#include "types.h"
#include "stat.h"
#include "user.h"
#include "psched.h"

int
main(int argc, char *argv[])
{
    struct pschedinfo st;
    int pid = getpid();
   
    if(getschedstate(&st) == 0) {
        for(int i = 0; i < NPROC; i++) {
            if (st.inuse[i] && st.pid[i] == pid) {
                if (st.priority[i] || st.nice[i]) {
                    printf(1, "XV6_SCHEDULER\t FAILED\n");
                    exit();
                }
            }
        }
    } else {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
        exit();
    }

    printf(1, "XV6_SCHEDULER\t SUCCESS\n");
    exit();
}
