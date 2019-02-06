/**
 * After its creation, it parses all the informations of "A" processes and try to swipe
 * the one that he consider being the best choice for having a child with a high value
 * for the genome.
 * After having swiped the other process, it will wait for an answer. In case of a positive
 * answer, it will notify the handler of their pid and then terminate, otherwise it keeps
 * swiping.
 */

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include "utils/headers/utils.h"

/* Global variables. We need them to be able to access them from any function */
struct node* node_element;
boolean waiting;
boolean waiting_handler; /* variable used to check if the handler already notified */
boolean sentenced_to_death;
struct match out_message;
struct swipe message;
struct memory_tree* tree;
struct node* memory_address;

/* Function declaration */
void interrupt_wedding(int );
void interrupt_handler(int );
void birth_death_handler(int );

int main(int argc, char* argv[]) {

    DEBUG_PRINT("[B] Starting B\n");

    int shm_id, counter=0;
    char* name;
    unsigned long genome;

    name = (char *) malloc(strlen(argv[1]) + 1);

    if(!name) {
        malloc_exception();
    }


    signal(SIGNAL_A_B, interrupt_handler);
    /* Signal for birth_death */
    signal(SIGINT, birth_death_handler);

    DEBUG_PRINT("[B] I'm in B with %d elements\n", argc);

    if (argc != 3) {
        argc_exception();
    }
    
    shm_id = atoi(argv[0]);
    strcpy(name, argv[1]);
    genome = atol(argv[2]);

    sentenced_to_death = FALSE;

    /* Attaching memory to process A */
    tree = (struct memory_tree *) shmat(shm_id, NULL, 0);

    /* Passing the list to our memory address */
    memory_address = tree->list;

    DEBUG_PRINT("[B] map_id %d name %s genome %lu\n", shm_id, name, genome);
    DEBUG_PRINT("[B] Process identifier: %d\n", getpid());

    node_element = memory_address;

    /* Main loop for the process to try and wait for the semaphore of process A
     * Where the light is green he will try to have a match with a process of type A
     * as long as he doesn't get accepted by someone or dies. */
    while(!sentenced_to_death) {

        DEBUG_PRINT("[B] I'm the process with pid: %d and counter: %d\n", getpid(), counter);

        /* Since with memset we put everything to 0, it would me invalid. It will
         * change only after initialization */
        if (node_element->pid && !node_element->is_dead) {

            DEBUG_PRINT("[B] Trying to signal [%d][%d]\n", node_element->pid, counter);

            /* Wait for the lock */
            if(gcd(genome, node_element->gene) > 1) {
                if(!sem_trywait(&node_element->mutex)) {
                    DEBUG_PRINT("[B] I acquired the lock: %d and my pid is: %d \n", counter, getpid());

                    DEBUG_PRINT("[B] Pid of A: %d\n", node_element->pid);
                    DEBUG_PRINT("[B] Genome of A: %lu\n", node_element->gene);
                    DEBUG_PRINT("[B] I'm in element: %d\n", counter);

                    message.pid = getpid();
                    message.genome = genome;

                    /* We write the message that is going to be sent to A and then wake
                        * him with kill sending alarm */
                    memcpy(&node_element->communication.swipe, &message, sizeof(struct swipe));
                    DEBUG_PRINT("[B] Write exit code: %d\n", errno);    
                    DEBUG_PRINT("[B] Sent ALARM to: %d\n", node_element->pid);
                    waiting = TRUE;
                    
                    kill(node_element->pid, SIGNAL_A_B);

                    /* Gonna be waiting now */
                    if (!node_element->is_dead) { 
                        while(waiting);
                    }
                    sem_post(&node_element->mutex);
                    DEBUG_PRINT("[B] I released the lock: %d\n", counter);

                    /* If accepted and I'm here, it means that I can now terminate */
                    if(out_message.answer) {
                        DEBUG_PRINT("[B] Going to process heaven, I am %d\n", getpid());
                        exit(0);
                    }
                }
            }
        }
            
        counter++;

        /* Integers are in such a way by which the last elements point to
         * the first element, therefore, when we get to the last element
         * we will already be pointing to the first element, therefore we
         * do not need to reset node_element */
        if(!node_element->next) {
            counter = 0;
        }

        DEBUG_PRINT("[B] Jumping to node %d. I have pid: %d\n", node_element->next, getpid());

        node_element = &memory_address[node_element->next];
        
    }

    return 0;
}

void interrupt_wedding(int sig_val) {
    DEBUG_PRINT("[B] Received wedding confirmation\n");
    waiting_handler = FALSE;
}

/* Handler for interruption. When the two processes tries to match, they communicate
 * with pipe. This means that, for every message, they must interrupt their waiting so
 * that they can decide whether is a match and, communicate the result to each other and
 * to the handler. */
void interrupt_handler(int sig_value) {
    
    DEBUG_PRINT("\n[B] B woke up. I am: %d\n", getpid());

    memcpy(&out_message, &node_element->communication.match, sizeof(struct match));
    DEBUG_PRINT("[B] The answer is: %d\n", out_message.answer);

    if((out_message.answer)) {
        DEBUG_PRINT("[B] Writing to handler\n");
        /* Communicate to handler */
        sem_wait(&tree->variables.message_to_handler.lock);

        /* Here it's assigned directly instead of memcpy since I have wedding only here
         * plus I don't want to risk of altering the lock
         */
        tree->variables.message_to_handler.sender = getpid();
        tree->variables.message_to_handler.pid_a = out_message.pid;
        tree->variables.message_to_handler.pid_b = getpid();

        /* Initialize the signal callback function that we use to wait handler response */
        signal(SIGNAL_WEDDING, interrupt_wedding);

        /* When the callback function gets invoked, the while will hand and it will be safe
         * to free the lock. Otherwise it may occur that "A" and "B" tries to write 
         * simultaneously since it's asyncronous */
        waiting_handler = TRUE;

        kill(getppid(), SIGNAL_WEDDING);
        while(waiting_handler);

        node_element->is_dead = TRUE;

        /* Free the lock */
        sem_post(&tree->variables.message_to_handler.lock);
    }

    waiting = FALSE;
    /* re-definition of the callback function for the signal */
    signal(SIGNAL_A_B, interrupt_handler);
    DEBUG_PRINT("[B] Returning in queue\n");
}

/* Function to handle the birth_death. If I get sentenced to death, I have to set the variable to true
 * so that, after the termination of the current lock, I can safely terminate. */
void birth_death_handler(int signal_received) {
    DEBUG_PRINT("[B] I must die. I am %d\n", getpid());
    sentenced_to_death = TRUE;
}
