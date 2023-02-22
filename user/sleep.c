#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// #include "dantynoel.c"

int main(int argc, char * argv[]){
    if (argc < 2){
        write(1, "Error: Missing Arguments\n", 26);
        exit(0);
    }
    int t;
    t = atoi(argv[1]);
    write(1, "\n", 1);
    sleep(t);

    exit(0);
}