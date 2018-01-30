# NAME:  Daxuan Shu
# Email: daxuan@g.ucla.edu
# ID:    204853061

CC = gcc
CFLAGS = -g -Wall -Wextra
UID = 204853061
TARBALL = lab1a-$(UID).tar.gz
SOURCE = lab1a.c Makefile README

default:
	@$(CC) $(CFLAGS) -o lab1a lab1a.c
clean:
	@rm -f *.o lab1a *.txt *.tar.gz
dist: default
	tar -czf $(TARBALL) $(SOURCE)

