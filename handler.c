/**
 * At the beginning, we create a number of init_people children. For each
 * child, we will randomly define:
 *  his type: “A” or “B”;
 *  his name: capital letter (from “A” to “Z”);
 *  his genome: an unsigned long from 2 to 2+genes.
 * Children are created with fork and execve.
 * Every birth_death milliseconds a process is deleted.
 * The duration is sim_time.
 */

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "utils/headers/utils.h"

struct process_info {
    pid_t pid;
    char type;
    char* name;
    unsigned long genome;
    struct process_info* next;
};

/* Function declaration */
void int_handler(int);
void wedding_handler(int);
/* Functions to create and remove elements from the list used for children generation */
struct process_info* helper_function(struct process_info* , struct process_info* );
struct process_info* add_new_element(struct process_info* , pid_t , char , char* , unsigned long );
struct process_info* remove_element(struct process_info* , pid_t );
void add_element(struct process_info* , struct process_info* );
struct process_info* pop_element(struct process_info* );
int list_length(struct process_info* );
struct process_info* pop_element_at_index(struct process_info* , int );
boolean is_present(struct process_info* , pid_t );
void post_simulation_kill();
void post_simulation_print();

/* Find the first free spot and adds the name */
void push_info(char** , char* , unsigned long);
/* Free the first non-empty spot */
char* pop_name(char**);

/* Definition of the global variables */
struct node* map;
struct memory_tree* tree;
struct process_info* process_list;
struct process_info* dead_process_list;

/* Setting the variable here as global to modify it in every point */
int child_counter;
/* Value for the shared memory */
int shm_id;
char** prefixes;
/* Array for pipes */
int* internal_pipe;

int main(int argc, char** argv) {
    /* First things first, I need to be sure that log.txt and sim.txt are not present since I will 
     * create them for simulation purpose */
    system("rm -f log.txt");
    system("rm -f sim.txt");
    system("clear");

    /* SIGKILL interrupt handler */
    signal(SIGKILL, int_handler);
    signal(SIGNAL_WEDDING, wedding_handler);

    int init_people, birth_death;
    int type_child_counter = 0;
    int sim_time;
    char* name;
    unsigned long genes;
    int map_cnt = 0;
    boolean spin_lock = FALSE;

    /* Setting the structures for the process information lists */
    process_list = add_new_element(NULL, 0, 0, NULL, 0);
    /* List for dead processes */
    dead_process_list = add_new_element(NULL, 0, 0, NULL, 0);
    
    /* Setting the counter for the children to 0 */
    child_counter = 0;

    /* Creation of the shared memory for the processes
     * Since we assume that we won't have more than a 1000 nodes plus some
     * global variables we are going to alloc such memory as follow */
    shm_id = create_shared_memory(sizeof (struct node) * MAXNODES +
                                    sizeof (struct global_var));
    tree = (struct memory_tree *) shmat(shm_id, NULL, 0);
    map = tree->list;

    if (argc != 5) {
        argc_exception();
    }

    /* Save value for argv */
    init_people = atoi(argv[1]);
    genes = atol(argv[2]);
    birth_death = atoi(argv[3]);
    sim_time = atoi(argv[4]);

    if(init_people < 2) {
        fprintf(stderr, "Invalid input for init_people\n");
        exit(-1);
    }

    prefixes = (char**) malloc(sizeof(char*) * init_people); 

    if(!prefixes) {
        malloc_exception();
    }

    internal_pipe = (int*) malloc(sizeof(int) * 2);
    if(pipe(internal_pipe) == -1) {
        pipe_exception();
    }

    if (fcntl(internal_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
        pipe_exception();
    }

    /* First of all we initialize the global semaphore into the init_people value, so that
     * when the last process is create it will be 0 and processes will be able to start
     * their lifecycle (1).
     * We then the semaphore for the communication between processes and handler (2) */
    sem_init(&tree->variables.process_blocker, 1, 0); // (1)
    sem_init(&tree->variables.message_to_handler.lock, 1, 1); // (2)

    DEBUG_PRINT("[H] init_people %d genes %lu birth_death %d %d\n", init_people, genes, birth_death, sim_time);

    /* Initialising the stack */
    for(int i = 0; i < init_people; i++) {
        add_new_element(dead_process_list, 0, 0, "", 2);
    }

    /* Usually we would use srand(time(NULL)) but, since we create
    * a whole copy, we would also copy the random seed generator
    * making a non random number. */
    srand(getpid());

    /* end_time and cur_time are defined as follows to allow us to have full control
     * to the time of the simulation */
    time_t cur_time = time(NULL);
    time_t end_time = cur_time + sim_time;
    time_t birth_death_time = cur_time + birth_death;

    DEBUG_PRINT("[H] il mio tempo e' %ld\n", cur_time);
    DEBUG_PRINT("[H] uccido in %ld\n", birth_death_time);
    DEBUG_PRINT("[H] finisco %ld\n", end_time);

    pid_t temp_element_pid = 0;
    
    while(cur_time < end_time) {
        cur_time = time(NULL); // time(NULL) set the current time

        pid_t pipe_element_pid = 0;

        if(read(internal_pipe[0], &pipe_element_pid, sizeof(pid_t)) && pipe_element_pid) {
            DEBUG_PRINT("[H] just read %d\n", pipe_element_pid);
            if(!temp_element_pid) {
                temp_element_pid = pipe_element_pid;
            }
            else {
                pid_t single_process = 0;
                pid_t node_pid = 0;
                boolean is_all_alive = TRUE;
                if(is_present(process_list, temp_element_pid)) {
                    single_process = temp_element_pid;
                } else {
                    is_all_alive &= FALSE;
                }

                if(is_present(process_list, pipe_element_pid)) {
                    single_process = pipe_element_pid;
                } else {
                    is_all_alive &= FALSE;
                }

                if (is_all_alive) {
                    /* Getting info for processA */
                    struct process_info* element_a = remove_element(process_list, temp_element_pid);
                    struct process_info* element_b = remove_element(process_list, pipe_element_pid);

                    assert(element_a);
                    assert(element_b);

                    DEBUG_PRINT("[H] Generating new parents %d %d %d %d\n", element_a->pid, element_b->pid, temp_element_pid, pipe_element_pid);

                    /* Using temp variables since using the genomes themselves would not work */
                    unsigned long genome_1 = element_a->genome;
                    unsigned long genome_2 = element_b->genome;

                    element_a->genome = element_b->genome = gcd(genome_1, genome_2);

                    add_element(dead_process_list, element_a);
                    add_element(dead_process_list, element_b);

                    if(element_a->type == 'A') {
                        node_pid = element_a->pid;
                    } else {
                        node_pid = element_b->pid;
                    }
                    
                    child_counter -= 2;
                } else if (single_process) {
                    /* single_process could be 0 if BOTH have been killed before marriage (high interleavings) */
                    struct process_info* single_element = remove_element(process_list, single_process);

                    add_element(dead_process_list, single_element);

                    if(single_element->type == 'A') {
                        type_child_counter--;
                    } else {
                        type_child_counter++;
                    }

                    node_pid = single_element->pid;

                    child_counter -= 1;
                }

                // re-initialise
                temp_element_pid = 0;

                struct node* iterator = &map[0];
                struct node* prev = iterator;
                DEBUG_PRINT("[H] Done a removal!\n");

                while (iterator->next && node_pid) {
                    DEBUG_PRINT("[H] Compare %d - %d\n", iterator->pid, node_pid);
                    if (iterator->pid == node_pid) {
                        prev->next = iterator->next;
                        break;
                    }
                    prev = iterator;
                    iterator = &map[iterator->next];
                }
            }
        }

        /* Every birth_death seconds we need to kill a process and create a new one.
         * We do it by passing the information into the list before killing it.
         * Furthermore, in the processes the killing code is placed so that it occurs after
         * every lock has been released. */
        if(cur_time >= birth_death_time) {
            int length = random_number(list_length(process_list));
            struct process_info* element_to_kill;

            DEBUG_PRINT("[H] having random length %d with full length %d\n", length, list_length(process_list));

            element_to_kill = pop_element_at_index(process_list, length);

            DEBUG_PRINT("[H] Dead eleent has type: %c\n", element_to_kill->type);

            if(element_to_kill->type == 'A') {
                type_child_counter--;
            } else {
                type_child_counter++;
            }

            DEBUG_PRINT("[H] Selected %d for birth death\n", element_to_kill->pid);

            /* Add void element for the creation of a new process */
            add_new_element(dead_process_list, 0, 0, "", 2);

            /* Update to User state of things */
            printf("\n******* Process %d has been killed after %d secs. Creating a new one *******\n\n", element_to_kill->pid, birth_death);

            kill(element_to_kill->pid, SIGINT);

            /* Reinstantiating birth_death time */
            birth_death_time = cur_time + birth_death;
            child_counter--;
        }

        /* Only if I generated enough child I will set the semaphore to init_people so that
         * they can start their lifecycle  */
        if(child_counter == init_people && !spin_lock) {
            spin_lock = TRUE;
            for(int i = 0; i < init_people; i++) {
                /* We decrement the semaphore up to 0 so that processes can start their
                 * lifecycle */
                sem_post(&tree->variables.process_blocker);
            }
        }
        
        while(list_length(process_list) < init_people) {

            for(struct process_info *iterator = process_list;
                        iterator;
                        iterator = iterator->next) {
                DEBUG_PRINT("[H] ---------- PID: %d\n", iterator->pid);
            }

            char type;
            
            /* if the type_child_counter is equals to init_people it means that only
            * one type of process has been generated and I will kill them all. To only check
            * once I make use of the abs function present in <stdlib.h> */
           DEBUG_PRINT("[H] ------------------------------ ho %d type e %d counter\n", type_child_counter, child_counter);
            if(type_child_counter >= (init_people - 1)) {
                DEBUG_PRINT("[H] Too many A processes, must use B\n");
                type = 'B';
            } else if (type_child_counter <= -(init_people - 1)) {
                DEBUG_PRINT("[H] Too many B processes, must use A\n");
                type = 'A';
            } else {
                DEBUG_PRINT("[H] Balanced number of processes, can go random\n");
                type = random_type();
            }

            /* We increment and decrement the value by 1 to check whether or not there
             * is only one type of process or a mixed amount. */
            if(type == 'A') {
                type_child_counter++;
            }
            else {
                type_child_counter--;
            }

            struct process_info* element_informations = NULL;
            element_informations = pop_element(dead_process_list);

            /* Random values for name and genome */
            name = process_name(element_informations->name);
            DEBUG_PRINT("[H] type %c name %s\n", type, name);
            unsigned long my_genome = random_genome(genes, element_informations->genome);

            DEBUG_PRINT("[H] Number of children is %d of %d\n", type_child_counter, child_counter);

            struct node* new_node;

            /* We check the type twice. The first time, now, is due to the
             * fact that we will generate structs only for type A that must
             * have all different address, we therefore must do it now to
             * ensure multiple different memory address (before the fork) */
            if(type == 'A') {

                new_node = &map[map_cnt];
                /* Set every elements of the node struct to 0 */
                memset(&map[map_cnt], 0, sizeof(struct node));

                /* We initialise now the semaphore for processes of type A */
                sem_init(&new_node->mutex, 1, 1);

                /* We make the new node to point to the root and then move
                 * the root there (aka head insertion) */
                struct node* iterator = &map[0];

                while(iterator->next) {
                    iterator = &map[iterator->next]; // map is the only array we got
                }

                iterator->next = map_cnt;

                DEBUG_PRINT("[H] New Node has address: %p. Index: %d\n", (void*) new_node, map_cnt);
            }

            pid_t pid = fork();

            if(pid < 0) {
                fork_exception();
            }
            else if(pid == 0) {
                /* Before the running phase of the processes I want to store their type, genome and name
                * into our "sim.txt" file. */
                SIM_PRINT("%d %c %lu %s\n", getpid(), type, my_genome, name);

                if(type == 'A') {
                    /* Adding gene to structure */
                    new_node->gene = my_genome;

                    /* I'm gonna pass the array as 4 arguments, with the
                     * last one null so that the process A will know when
                     * there are no more elements. */
                    char* arguments[5];

                    arguments[0] = "./processA";

                    arguments[1] = (char *) malloc (sizeof(char) * 20);

                    if(!arguments[1]) {
                        malloc_exception();
                    }

                    sprintf(arguments[1], "%d", shm_id);
                    
                    arguments[2] = (char *) malloc (strlen(name) + 1);

                    if(!arguments[2]) {
                        malloc_exception();
                    }

                    strcpy(arguments[2], name);

                    arguments[3] = (char *) malloc (sizeof(char) * 20);

                    if(!arguments[3]) {
                        malloc_exception();
                    }

                    sprintf(arguments[3], "%d", map_cnt);

                    DEBUG_PRINT("[F] I'm generating a A process with pid: %d\n", getpid());
                    DEBUG_PRINT("[F] map_id %d name %s map_counter %d\n", shm_id, name, map_cnt);

                    char* const params[] = {arguments[1], arguments[2], arguments[3], NULL};

                    execve(arguments[0], params, NULL);

                    exit(1);
                } 
                else if (type == 'B') {
                    /* I'm gonna pass the array as 4 arguments, with the
                     * last one null so that the process B will know when
                     * there are no more elements. */
                    char* arguments[5];

                    arguments[0] = "./processB";

                    arguments[1] = (char *) malloc (sizeof(char) * 20);

                    if(!arguments[1]) {
                        malloc_exception();
                    }

                    sprintf(arguments[1], "%d", shm_id);
                    
                    arguments[2] = (char *) malloc (strlen(name) + 1);

                    if(!arguments[2]) {
                        malloc_exception();
                    }

                    strcpy(arguments[2], name);

                    arguments[3] = (char *) malloc (sizeof(long) * 2);

                    if(!arguments[3]) {
                        malloc_exception();
                    }

                    snprintf(arguments[3], sizeof(arguments[3]), "%lu", my_genome);

                    DEBUG_PRINT("[F] I'm generating a B process with pid: %d\n", getpid());
                    DEBUG_PRINT("[F] map_id %d name %s map_counter %d\n", shm_id, name, map_cnt);

                    char* params[] = {arguments[1], arguments[2], arguments[3], NULL};

                    execve(arguments[0], params, NULL);

                    exit(1);
                }
            }
            else {
                add_new_element(process_list, pid, type, name, my_genome);
            }

            if(type == 'A') {
                map_cnt++;
            }

            child_counter++;
        }
    }

    DEBUG_PRINT("[H] il mio tempo adesso e' %ld\n", cur_time);
    DEBUG_PRINT("[H] uccido in %ld\n", birth_death_time);
    DEBUG_PRINT("[H] finisco %ld\n", end_time);

    int_handler(0);

    DEBUG_PRINT("[H] child_counter %d init_people %d\n", child_counter, init_people);
    return 0;
}

void int_handler(int dummy) {
    post_simulation_kill();
    post_simulation_print();
    shmctl(shm_id, IPC_RMID, NULL); // free the shared memory
}

void wedding_handler (int _sigval) {
    DEBUG_PRINT("\n[H] They woke me up\n");

    //struct wedding temp_structure;
    struct wedding* temp_structure = (struct wedding*) malloc(sizeof(struct wedding));

    if(!temp_structure) {
        malloc_exception();
    }

    temp_structure = &tree->variables.message_to_handler;

    DEBUG_PRINT("[H] Input data pid: A: %d, B: %d\n", temp_structure->pid_a, temp_structure->pid_b);

    /* Instantiating a new struct of type node so that I don't have to "touch" map */
    struct node* iterator = &map[0];

    while(iterator->pid != temp_structure->pid_a) {
        /* BEWARE: this could process infinitely if it doesn't find anything */
        iterator = &map[iterator->next];
    }

    if(iterator->pid_matched && iterator->pid_matched == temp_structure->pid_b) {
        /* handler cannot kill processes since we may encounter problems (plus in the 
         * specifications it said that the processes needed to terminate themselves) */
        printf("******* MARRIAGE BETWEEN: %d and %d *******\n", temp_structure->pid_a, temp_structure->pid_b);
        DEBUG_PRINT("******* MARRIAGE BETWEEN: %d and %d *******\n", temp_structure->pid_a, temp_structure->pid_b);

	    DEBUG_PRINT("[H] going to write %d and %d\n", iterator->pid, iterator->pid_matched);
	    write(internal_pipe[1], &iterator->pid, sizeof(pid_t));
	    write(internal_pipe[1], &iterator->pid_matched, sizeof(pid_t));	
    }

    iterator->pid_matched = temp_structure->pid_b;

    /* re-definition of the callback function for the signal */
    signal(SIGNAL_WEDDING, wedding_handler);

    /* We can signal to the processes that they can terminate */
    kill(temp_structure->sender, SIGNAL_WEDDING);
}

/* Helper function invoked when adding elements to a list of struct process_int* */
struct process_info* helper_function(struct process_info* list, struct process_info* new_element) {

    /* Make sure that the next element is NULL */
    new_element->next = NULL;

    /* If we have an existing list we append the element in it, otherwise we create a new
     * one. */
    if (list) {
        while(list->next) {
            DEBUG_PRINT("[H] Element: (%d)%s with genome %lu\n", list->pid, list->name, list->genome);
            list = list->next;
        }
        DEBUG_PRINT("[H] Exit loop\n");
        DEBUG_PRINT("[H] Element: (%d)%s with genome %lu\n", list->pid, list->name, list->genome);

        list->next = new_element;
    } else {
        list = new_element;
    }

    DEBUG_PRINT("[H] Return helper\n");
    return list;
}

/* Function used to add an element into the list of processes */
struct process_info* add_new_element(struct process_info* list, pid_t pid, char type, char* name, unsigned long genome) {

    struct process_info* new_element = (struct process_info*) malloc(sizeof(struct process_info));

    if(!new_element) {
        malloc_exception();
    }

    new_element->pid = pid;
    new_element->type = type;
    new_element->name = name;
    new_element->genome = genome;

    return helper_function(list, new_element);
}

/* Function used to remove an element into the list of processes.
 * We achieved this by creating a pointer to the previous element and making it point to the
 * element next to the current one. */
struct process_info* remove_element(struct process_info* list, pid_t pid) {
    /* Definition of a pointer for the previous step */
    struct process_info* previous = NULL;

    DEBUG_PRINT("[H] going to remove %d\n", pid);

    while(list) {
        if(list->pid == pid) {
            if (previous) {
                previous->next = list->next;
                return list;
            } 
        }

        previous = list;
        list = list->next;
    }

    DEBUG_PRINT("[H] Failed removal of %d\n", pid);

    /* Empty list. Unreachable */
    assert(FALSE);
}

/* Simple function used to add an element into the list by using only the helper_function */
void add_element (struct process_info* list, struct process_info* element) {
    helper_function(list, element);

    return;
}

/* With pop_element we get the last element from the list. */
struct process_info* pop_element (struct process_info* list) {
    if (!list) {
        assert(FALSE);
    }

    struct process_info* original_list = list;

    while(list->next) {
        list = list->next;
    }

    remove_element(original_list, list->pid);

    return list;
}

/* Function that returns the length of the list by returning an integer incremented on every
 * iteration of the elements of the list */
int list_length(struct process_info* list) {
    int length=0;

    while (list) {
        if (list->pid) {
            length++;
        }
        list = list->next;
    }

    return length;
}

/* Function that pop a specific element of the list at a given index.
 * For simplicity, we treat the list as an array, by using a temp value that works to check 
 * if he got to the correct index of the list. */
struct process_info* pop_element_at_index (struct process_info* list, int index) {
    int i=-1; /* Since the first element is always 0, I have to make sure not to select that one */ 

    assert(list);

    struct process_info* original_list = list;

    while(list && i != index) {
        list = list->next;
        i++;
    }

    /* In case index is out of bounds, the last element of the list will be removed */
    remove_element(original_list, list->pid);

    return list;
}

boolean is_present(struct process_info* list, pid_t pid) {
    while(list) {
        if(list->pid == pid) {
            return TRUE;
        } 

        list = list->next;
    }

    return FALSE;
}

/* Killing all subprocess */
void post_simulation_kill() {
    /* for loop for the elimination of the processes. This is a C++ style */
    for(struct process_info* iterator = process_list;
                iterator;
                iterator = iterator->next) {
        if(iterator->pid) {
            kill(iterator->pid, SIGKILL);
        }
    }
}

/* Function that parses the sim.txt file to read data for the final print to user */
void post_simulation_print() {
    FILE* fin = fopen(SIM_FILE, "r");

    if(!fin) {
        file_exception();
    }

    int process_counter=0;
    pid_t max_pid_g, max_pid_n, pid;
    unsigned long max_genome, genome;
    char max_name[2000];
    char name[2000];
    char type;

    memset(max_name, 0, 2000);
    memset(name, 0, 2000);

    /* Intialising default values */
    max_genome = 0;
    max_pid_g = 0;
    max_pid_n = 0;
    //max_name = 0;

    while(fscanf(fin, "%d %c %lu %s\n", &pid, &type, &genome, name) != EOF) {
        process_counter++;

        if(genome > max_genome) {
            max_genome = genome;
            max_pid_g = pid;
        }

        if(strlen(name) > strlen(max_name)) {
            strcpy(max_name, name);
            max_pid_n = pid;
        }
    }

    printf("The number of generated process was: %d\n", process_counter);
    printf("The longest name for a process was: %s with length: %lu and PID: %d\n", max_name, strlen(max_name), max_pid_n);
    printf("The greatest genome was: %lu with PID: %d\n", max_genome, max_pid_g);
}
