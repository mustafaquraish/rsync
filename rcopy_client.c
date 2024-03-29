#include <stdio.h>
#include "ftree.h"

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage:\n\trcopy_client SRC DEST IP\n");
        return 0;
    }

    int ret = fcopy_client(argv[1], argv[2], argv[3], PORT);
    if (ret == 1) {
        printf("Errors encountered during copy\n");
        ret = -ret;
    } else {
        printf("Copy completed successfully\n");
    }
    
    return 0;
}
