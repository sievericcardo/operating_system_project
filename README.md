# Operating System project report

## Abstract

---

The project structure was about the implementation of a piece of software that generates and manage processes and inter-process-communication.

## Project structure

---

The process that manage the whole project is the handler. I execute the handler when starting the execution of the software and it will execute the other two processes ("A" and "B"). We also get, from command line:

- *init_people*: number of processes I will generate through handler --- this number indicates the number of processes alive concurrently.
- *genes*: value used as range to generate random value for the genome.
- *birth_death*: every birth_death seconds, I kill a random process and generate a new one.
- *sim_time*: simulation time expressed in seconds.

Whenever I create a new process, I assign a random genome --- its value have $x$ to $x + genes$ value, where $x$ is $0$ when the simulation starts.
I give each process a name, by appending a random character to an array --- initially empty. I developed the function that handles the process naming in such a way to ensure major efficiency when generating children of children.
I handled process types as:

- If, up to the penultimate element, I only have "B" elements, I will need to generate a "B" process --- it would block otherwise
- If, up to the penultimate element, I only have "A" elements, I will need to generate a "A" process
- Random otherwise

The handler generates new processes by forking, generating new values for the process and execute them by passing the values via *execve*

## Processes life-cycle

---

### Process A

Processes of type A wait to communicate with B processes. They decide wether or not to match with them based on two factors:

1. If process B's genome is a multiple of the process A's one they match
2. If $gcd(genomeA, genomeB) > \frac{genomeA}{adaptability\_factor}$ where the *adaptability_factor* is the value that adapt the preference of a process A when choosing a partner. Initially set to $1$, when it refuses $5$ a process B it will be easier to match --- and avoid being lonely forever.

### Process B

Process of type B try to get in contact with process A in order to have a match. They evaluate the $gcd$ of both their genome and process A's one to ensure their children will have a suitable genome --- they check $gcd > 1$ --- then start contacting the relative process A. To avoid that more than one process of type B get in touch with the same process of type A, I implemented a semaphore system.
Both processes, when matched, will communicate the handler their information, then terminate. The handler will save the incoming data to generate children accordingly:

- *name*: the child's name will be generated from the parents' name with a random character appended.
- *genome*: the genome is a random value in the range $x$ to $x+genes$ where $x$ is the *gcd* of the parents' genomes.

## Semaphores

---

I implemented the semaphores, throughout the project, with POSIX semaphores generating three different kind of semaphores:

1. The first one handles the initialization of the processes. I block them until we have generated all the initial processes --- whose number is defined with *init_people* --- and then release the lock.
2. The second one handles the processes' life-cycle. It prevents process B to try establishing a communication with a process of type A, if it's already busy
3. The last semaphores handle the communication between processes and the handler itself. When a process get a match, both processes will communicate with the handler before terminating. When this happens, they will need to get the lock on the semaphore, communicate the handler its information, and then release the semaphore.

### POSIX semaphores vs System V semaphores

As previously mentioned, I opted for POSIX semaphores over System V one. The main reasons for this choice were:

- System V semaphores con increment or decrement the semaphore counter of a specified value, whereas the POSIX one always perform this task of a single unit. As such, POSIX semaphores were easier to maintain.
- POSIX semaphores do not allow permission manipulation --- easier to maintain if not needed.
- They are relatively lighter and have an overall better performance.
- POSIX semaphores are non persistent --- this may be a problem if not well controlled

I also found POSIX semaphores to be, after all, easier to implement and use, especially when using the function `sem_trywait` for an asyncronous task.

<!-- 
		WIP sections

## Shared Memory management

### Inter-process communication

## Makefile structure and Debug print

## Post-simulation outcomes

## Possible runtime errors

-->
