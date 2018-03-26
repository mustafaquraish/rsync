#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hash.h"

void hashAux(char *hash_val, long block_size, FILE* f) {
    int i;
    char c;

    for (i = 0; i < block_size; i++){
        hash_val[i] = '\0';
    }

    i=0;
    while(fscanf(f, "%c", &c) > 0){
        hash_val[i] ^= (char)c;
        i = (i+1)%block_size;
    }
}

char *hash(FILE* f){
	char *hash = malloc(sizeof(char) * HASH_SIZE);

	hashAux(hash, 8, f);
	return hash;
}
