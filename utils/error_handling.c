#include "headers/error_handling.h"

void exception(char* message) {
    int errnum = errno;
    fprintf(stderr, "%s: %s\n", message, strerror( errnum ));
    exit(EXIT_FAILURE);
}

void file_exception() {
    exception("Error opening file\n");
}

void scanf_exception() {
    exception("Error while retreiving data\n");
}

void argc_exception() {
    exception("Error in the number of cl arguments\n");
}

void fork_exception() {
    exception("Error while attempting to fork\n");
}

void malloc_exception() {
    exception("Error while allocating memory\n");
}

void pipe_exception() {
    exception("Error! Pipe failed!\n");
}
