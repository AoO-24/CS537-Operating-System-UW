#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{    

    int pid = fork();

    if(pid == 0){
    // child
        char *argv[2];
        argv[0] = "cat";
        argv[1] = "README";
        exec("/cat", argv);
        exit();
        
        
    }
    if(pid > 0) {
        wait();
        int pid = fork();
        if(pid == 0){
        // child
            char *argv[1];
            argv[0] = "getlastcat";
            exec("/getlastcat", argv);
            exit();
        }
        if(pid > 0) {
            wait();
            exit();
        }
    }    
}