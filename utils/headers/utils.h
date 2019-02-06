/**
 * Library with utilities.
 * We define the function for the mapping of the shared memory, the generation
 * of random numbers plus extra functions that are loaded from other libraries.
 * The idea of using extra libraries inside the utils library is to ensure
 * extreme modularity and separate parts that would require implementations
 * on its own inside here, so that from the core part of the application
 * we just need to recall utils.h
 * An example is the error handling library.
 */

#ifndef UTILS_INCLUDED
#define UTILS_INCLUDED

/* Custom boolean type */
typedef enum {FALSE, TRUE} boolean;

#define MAXNODES 10000
#define SIGNAL_A_B SIGUSR1
#define SIGNAL_WEDDING SIGUSR2

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>

#include "error_handling.h"

/* Macros for specific prints.
 * We got multiple level of print. We shall use, as default, the option of printing to file
 * all the print statement.
 * If we use DEBUG_ENABLED to 1, we will then print to standard output eveything, if we set it
 * to 0, we won't print any of them */
#define DEBUG_ENABLED 2

/* The macro below are done to print in debug mode. For the print to file, it is done by passing
 * a file that gets closed and reopened every time. As such, we must open it in append mode since
 * using a simple write would override every thing that was inserted. 
 * We use variadic MACROS for the debug print. They are simple macros in which the number of arguments
 * that will be passed are unknow in priori and can change from time to time (we need stdarg.h). */
#if DEBUG_ENABLED == 1 // simple debug to stdout
    #define DEBUG_PRINT(...) printf (__VA_ARGS__)
#elif DEBUG_ENABLED == 2 // debug to log.txt
    #define DEBUG_PRINT(...) \
        do { \
            FILE* fp = fopen ("log.txt", "a"); \
            fprintf(fp, __VA_ARGS__); \
            fclose(fp); \
        } while (0);
#else
    #define DEBUG_PRINT(...) do {} while (0)
#endif

#define SIM_FILE "sim.txt"

#define SIM_PRINT(...) \
    do { \
        FILE* fin = fopen (SIM_FILE, "a"); \
        fprintf(fin, __VA_ARGS__); \
        fclose(fin); \
    } while (0);

#include <semaphore.h>

/* Welcome to the tinder IPC */

/* B tries to swipe A */
struct swipe {
    pid_t pid;
    unsigned long genome;
};

/* A checks if it's a good partner or not */
struct match {
    pid_t pid;
    boolean answer;
};

/* Union for the message */
union message_system {
    struct swipe swipe;
    struct match match;
};

struct wedding {
    pid_t sender; // We send the sender pid for the signals
    pid_t pid_a;
    pid_t pid_b;
    sem_t lock;
};

/* Structure for global variables for processes to wait until every process are created */
struct global_var {
    sem_t process_blocker;
    struct wedding message_to_handler;
};

/* List for the struct node */
struct node {
    pid_t pid;
    pid_t pid_matched; // I will use this pid to know when to kill processes
    /* Since the pid of process A must be read, it is not safe to set pid to 0
        * therefore it's safer to use a flag that every process of type "B"
        * must check. */
    boolean is_dead;
    unsigned long gene;
    int next; // Index of the array of struct that we will create
    union message_system communication;
    sem_t mutex; // Variable used to initialise the semaphore
};

/* Structure for the memory tree */
struct memory_tree {
    struct global_var variables;
    struct node list[MAXNODES];
};

int create_shared_memory(size_t);
int random_number(int);
char random_type();
char random_name();
char* process_name(char* );
int random_genome(int, int);
boolean is_multiple(unsigned long, unsigned long);
unsigned long gcd(unsigned long, unsigned long );

#endif
