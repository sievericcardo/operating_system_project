HAND_EXEC= handler
A_EXEC= processA
B_EXEC= processB

C= gcc
CFLAGS= -std=c99 -pedantic -Werror -D_POSIX_C_SOURCE
CLINK= -lpthread -lrt # need these for POSIX semaphores

HAND=$(C) $(CFLAGS) handler.c utils/utils.c utils/error_handling.c -o $(HAND_EXEC)
A=$(C) $(CFLAGS) processA.c utils/utils.c utils/error_handling.c -o $(A_EXEC)
B=$(C) $(CFLAGS) processB.c utils/utils.c utils/error_handling.c -o $(B_EXEC)

HANDLER=$(HAND) $(CLINK)
P_A=$(A) $(CLINK)
P_B=$(B) $(CLINK)

.PHONY: clean

all:
	$(HANDLER) && $(P_A) && $(P_B)

clean:
	rm -rf $(HAND_EXEC) $(A_EXEC) $(B_EXEC)

run:
	./$(HAND_EXEC) 5 15 5 10

