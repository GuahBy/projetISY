CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# Fichiers objets communs
COMMON_OBJS = ipc.o network.o user.o group.o message.o utils.o

# Cibles
all: server client

server: server.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o server server.o $(COMMON_OBJS)

client: client.o $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o client client.o $(COMMON_OBJS)

# Compilation des fichiers objets
%.o: %.c messaging.h
	$(CC) $(CFLAGS) -c $<

# Nettoyage
clean:
	rm -f *.o server client
	rm -f shm_key.txt sem_key.txt

# Nettoyage des IPCs
cleanipcs:
	ipcrm -a

# Exécution
run-server: server
	./server

run-client: client
	./client

.PHONY: all clean cleanipcs run-server run-client
