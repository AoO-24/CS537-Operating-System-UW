#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{
    char buf[512];
    if (getlastcat(buf) < 0)
    {
        printf(1, "Failed to get the last catted filename\n");
    }
    else
    {
        printf(1, "XV6_TEST_OUTPUT Last catted filename: %s\n", buf);
    }

    exit();
}
