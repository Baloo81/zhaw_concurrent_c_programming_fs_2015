all: 
	gcc server.c -o server -std=gnu99 
	gcc client.c -o client 