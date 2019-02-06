#include "headers/utils.h"

int create_shared_memory(size_t size) {
    return shmget(IPC_PRIVATE, size, IPC_CREAT | 0666);
}

int random_number(int number) {
    return rand() % number;
}

char random_type() {
    return random_number(2) + 'A';
}

char random_name() {
    return random_number(26) + 'A';
}

/* Function that creates a new name for the process */
char* process_name(char* name) {
    DEBUG_PRINT("[P-N] I'm in process name\n");
    /* Generate new char */
    char str[2] = "\0"; // we always need the null terminating character
    str[0] = (random_number(26) + 'A');

    /* generate output string */
    char* new_name = (char *) malloc(strlen(name) + 2);

    /* Copy into output string */
    strcpy(new_name, name);
    strcat(new_name, str);

    DEBUG_PRINT("[P-N] new char: %s\n", str);
    DEBUG_PRINT("[P-N] name: %s\n", new_name);

    return new_name;
}

int random_genome(int genome, int x) { 
    return random_number(genome) + x;
}

/* Check if genome B is multiple of genome A */
boolean is_multiple(unsigned long genomeA, unsigned long genomeB) {
    return (genomeB % genomeA == 0);
}

unsigned long gcd(unsigned long genomeA, unsigned long genomeB) {
    if(genomeB != 0) {
        return gcd(genomeA, genomeA % genomeB);
    }
    else {
        return genomeA;
    }
}
