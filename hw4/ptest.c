#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
 
int 
main(void) {
    char *argv[] = {
        "ls"
    };

    char *path = "ls";

    int fds[16] = {-1};

    close(1);
    open("new.txt", O_CREATE|O_RDWR);
    fds[0] = 0;
    fds[1] = 1;
    fds[2] = 2;

    pcreate(path, argv, fds);
    wait();

    exit();
 }