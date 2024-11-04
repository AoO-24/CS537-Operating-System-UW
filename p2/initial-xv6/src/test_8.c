#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(void)
{

    int pid = fork();

    if (pid == 0)
    {
        // child
        char *argv[2];
        argv[0] = "cat";
        argv[1] = "README";
        exec("/cat", argv);
        int fd = open("testingfile", O_CREATE | O_RDWR);
        if (fd >= 0)
        {
            printf(1, "ok: create backup file succeed\n");
        }
        else
        {
            printf(1, "error: create backup file failed\n");
        }
        exit();
    }
    if (pid > 0)
    {
        wait();
        char message[256] = {"\0"};
        getlastcat(message); //&message[0]
        printf(1, "\n\nXV6_TEST_OUTPUT Last catted filename: %s\n", message);
        exit();
    }
}