.phony all:
all: MFS

MFS: MFS.c
	gcc -Wall -g MFS.c -lpthread -o MFS 

.PHONY clean:
clean:
	-rm -rf *.o *.exe
