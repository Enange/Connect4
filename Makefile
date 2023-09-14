CFLAGS=-Wall -std=gnu99
INCLUDES= -I./inc

all: F4Client F4Server

F4Server: Server.o errExit.o Semaphore.o
	gcc -g Server.o errExit.o Semaphore.o -o $@

Server.o: Server.c errExit.o Semaphore.o
	gcc -c -g $(INCLUDES) Server.c -o Server.o


F4Client: Client.o errExit.o Semaphore.o
	gcc -g Client.o errExit.o Semaphore.o -o $@

Client.o: Client.c errExit.o Semaphore.o
	gcc -c -g $(INCLUDES) Client.c -o Client.o


errExit.o: errExit.c 
	gcc -c -g $(INCLUDES) errExit.c -o errExit.o

Semaphore.o: Semaphore.c 
	gcc -c -g $(INCLUDES) Semaphore.c -o Semaphore.o

clean:
	rm -rf errExit.o Server.o Semaphore.o Client.o F4Client F4Server