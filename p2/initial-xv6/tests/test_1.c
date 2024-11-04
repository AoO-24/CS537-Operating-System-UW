#include "types.h"
#include "stat.h"
#include "user.h"

int main(void)
{    
    char message[256] = {"\0"};  
    getlastcat(message); //&message[0]
    printf(1,"XV6_TEST_OUTPUT Last catted filename: %s\n",message);
    exit();
}