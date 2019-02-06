/**
 * All the information of the process of type "A" are accessible by anyone (we used the node
 * struct). It waits to be contacted by a process of type "B" and then decides whether to
 * match or not.
 * In a positive case, it signals the choice to the other process and then communicate to the
 * handler their pid and then it terminates.
 * In a negative case, it will inform "B" and get in wait again to be awaken by another "B".
 * After a period of time it will adapt his criteria so that, in the end, it will match 
 * someone.
 */

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include "utils/headers/utils.h"

/* Function declaration */
boolean is_match(unsigned long, unsigned long);
void interrupt_wedding(int );
void interrupt_handler(int );
void birth_death_handler(int );

/* Global variables. We need them to be able to access them from any function */
char* name;
boolean waiting;
boolean waiting_handler; /* variable used to check if the handler already notified */
boolean answer; /* I need to be global to know whether I can kill A or not */ 
boolean sentenced_to_death;
unsigned long adaptability_factor;
unsigned long genome;
int swipe_attempt;
struct swipe in_message;
struct match out_message;
struct memory_tree* tree;
struct node* my_node;
struct node* memory_address;

int main(int argc, char** argv) {

    /* Definition of the variables for the memory and the structures */
    DEBUG_PRINT("[A] Starting A with pid: %d\n", getpid());

    int shm_id, index_to_gene;

    /* Initialization of the global variables for adaptability */
    adaptability_factor = 1;
    swipe_attempt = 0;
    
    name = (char *) malloc(strlen(argv[1]) + 1);

    if(!name) {
        malloc_exception();
    }


    /* Define the signal that will end the while */
    signal(SIGNAL_A_B, interrupt_handler);
    /* Signal for birth_death */
    signal(SIGINT, birth_death_handler);

    if (argc != 3) {
        argc_exception();
    }

    DEBUG_PRINT("[A] argv1 is: %s\n", argv[1]);
    
    shm_id = atoi(argv[0]);
    strcpy(name, argv[1]);
    index_to_gene = atoi(argv[2]);

    sentenced_to_death = FALSE;

    /* Attaching memory to process A */
    tree = (struct memory_tree *) shmat(shm_id, NULL, 0);

    /* Passing the list to our memory address */
    memory_address = tree->list;

    DEBUG_PRINT("[A] I am process %d with name %s and counter: %d\n", getpid(), name, index_to_gene);

    /* Derefering my memory address struct into a new pointer */
    my_node = &memory_address[index_to_gene];

    /* Get genome from structure */
    genome = my_node->gene;

    /* Setting the process ID of the structure to the curret pid of the process A */
    my_node->pid = getpid();

    waiting = TRUE;

    /* Main loop for the process to wait until he gets awaken by a process B
     * In that case it will operate checking whether to accept B or not and signaling
     * the choice to it. */
    while(!sentenced_to_death) {

        /* If "A" accepts "B", it can terminate */
        if(answer) {
            DEBUG_PRINT("[A] Going to process heaven, I am %d\n", getpid());
            exit(0);
        }

        waiting = TRUE;
    }

    return 0;
}

boolean is_match(unsigned long genomeA, unsigned long genomeB) {

    if(is_multiple(genomeA, genomeB)) {
        return TRUE;
    }

    if(gcd(genomeA, genomeB) > (genomeA / adaptability_factor)) {
        return TRUE;
    }
    
    return FALSE;
}

void interrupt_wedding(int sig_val) {
    DEBUG_PRINT("[A] Received wedding confirmation\n");

    waiting_handler = FALSE;
}

void interrupt_handler(int sig_value) {

    /* If I refused 5 attempt I try to make it easier to get called */
    if(++swipe_attempt >= 5) {
        adaptability_factor++;
    }
    
    DEBUG_PRINT("\n[A] A woke up. I am: %d\n", getpid());
    
    memcpy(&in_message, &my_node->communication.swipe, sizeof(struct swipe));
    DEBUG_PRINT("[A] I just received the genome %lu from %d\n", in_message.genome, in_message.pid);

    answer = is_match(my_node->gene, in_message.genome);

    /* Here there's no really need for memcpy. In addition, this way we can avoid
     * any chance to alter in any way the lock. */
    out_message.pid = getpid();
    out_message.answer = answer;
    memcpy(&my_node->communication.match, &out_message, sizeof(struct match));

    /* Check for compability */
    if(answer) {
        DEBUG_PRINT("[A] A [%d] accepts B\n", getpid());
        /* Communicate to handler */
        sem_wait(&tree->variables.message_to_handler.lock);

        /* Here it's assigned directly instead of memcpy since I have wedding only here
         * plus I don't want to risk of altering the lock
         */
        tree->variables.message_to_handler.sender = getpid();
        tree->variables.message_to_handler.pid_a = getpid();
        tree->variables.message_to_handler.pid_b = in_message.pid;

        /* Initialize the signal callback function that we use to wait handler response */
        signal(SIGNAL_WEDDING, interrupt_wedding);

        /* When the callback function gets invoked, the while will hand and it will be safe
         * to free the lock. Otherwise it may occur that "A" and "B" tries to write 
         * simultaneously since it's asyncronous */
        waiting_handler = TRUE;

        kill(getppid(), SIGNAL_WEDDING);
        
        while(waiting_handler);

        /* Free the lock */
        sem_post(&tree->variables.message_to_handler.lock);
    }
    else {
        DEBUG_PRINT("[A] A [%d] refuses B\n", getpid());
    }

    waiting = FALSE;
    /* re-definition of the callback function for the signal */
    signal(SIGNAL_A_B, interrupt_handler);

    DEBUG_PRINT("[A] Resetting process %d\n", getpid());

    kill(in_message.pid, SIGNAL_A_B);
}

/* Function to handle the birth_death. If I get sentenced to death, I have to set the variable to true
 * so that, after the termination of the current lock, I can safely terminate. */
void birth_death_handler(int signal_received) {
    DEBUG_PRINT("[A] I must die. I am %d\n", getpid());
    sem_wait(&my_node->mutex);

    sentenced_to_death = TRUE;
    my_node->is_dead=TRUE;

    sem_post(&my_node->mutex);
}
