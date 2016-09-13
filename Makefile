.PHONY : all clean

# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -Wall

all: server client server_select

server: server.c packet.c packet.h
	$(CC) $(CFLAGS) -o server server.c packet.c

client: client.c packet.c packet.h
	$(CC) $(CFLAGS) -o client client.c packet.c

server_select: server_select.c packet.c packet.h
	$(CC) $(CFLAGS) -o server_select server_select.c packet.c

check: server client server_select
	./client -h 143.248.56.16 -p 3000 -o 0 -s 100000 < sample/burrito.txt > 1.txt
	./client -h localhost -p 3000 -o 0 -s 100000 <sample/burrito.txt > 2.txt
	diff 1.txt 2.txt
	./client -h 143.248.56.16 -p 3000 -o 0 -s 100000 < sample/crossbow.txt > 1.txt
	./client -h localhost -p 3000 -o 0 -s 100000 <sample/crossbow.txt > 2.txt
	diff 1.txt 2.txt
	./client -h 143.248.56.16 -p 3000 -o 0 -s 100000 < sample/sample.txt > 1.txt
	./client -h localhost -p 3000 -o 0 -s 100000 <sample/sample.txt > 2.txt
	diff 1.txt 2.txt




clean:
	$(RM) server client server_select
