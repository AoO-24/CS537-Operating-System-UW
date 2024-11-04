#include "types.h"
#include "stat.h"
#include "user.h"
#include "psched.h"

int
main(int argc, char *argv[])
{
    if (getschedstate((struct pschedinfo *)1000000) != -1) {
        printf(1, "XV6_SCHEDULER\t FAILED\n");
    } else {
        printf(1, "XV6_SCHEDULER\t SUCCESS\n");
    }
    exit();
}
