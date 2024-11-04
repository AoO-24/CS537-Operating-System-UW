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
        char message[256] = {"\0"};  
        getlastcat(message); //&message[0]
        printf(1,"XV6_TEST_OUTPUT Last catted filename: %s\n",message);
        exit();
    }    
}