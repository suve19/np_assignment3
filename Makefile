CC = gcc
CC_FLAGS = -w -g
 
 
 
all: test 


main.o: main.cpp
	$(CC) -Wall -c main.cpp -I.


test: main.o
	$(CC) -L./ -Wall -o test main.o 


clean:
	rm *.o *.a test server client
