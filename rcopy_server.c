#include <stdio.h>
#include "ftree.h"

int main(int argc, char **argv) {
    if (argc != 1) {
        printf("Usage:\n\rcopy_server\n");
        return -1;
    }

    fcopy_server(PORT);
    return 0;
}
