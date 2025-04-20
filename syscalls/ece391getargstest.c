#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"


int main () {
    int32_t addr;
    uint8_t buf[1024];
    addr = 1;


    ece391_fdputs (1, (uint8_t*)"If you see any messages after this, syscall_getargs failed\n");
    
    if(ece391_getargs (0, 1024) == 0) {
        ece391_fdputs (1, (uint8_t*)"getargs failed with buf = NULL\n");
    }
    if(ece391_getargs (buf, 0) == 0) {
        ece391_fdputs (1, (uint8_t*)"getargs failed with nbytes = 0\n");
    }
    if(ece391_getargs ((uint8_t*)addr, 1024) == 0) {
        ece391_fdputs (1, (uint8_t*)"getargs failed due to buffer being outside of user = 1\n");
    }

    return 0;
}
