# NAME:  Daxuan Shu
# Email: daxuan@g.ucla.edu
# ID:    2*******1

CC = gcc
CFLAGS = -g -Wall -Wextra -lz
UID = 204853061
TARBALL = lab1b-$(UID).tar.gz
SOURCE = lab1b-client.c lab1b-server.c README  Makefile

default:
	@$(CC) $(CFLAGS) -o lab1b-client lab1b-client.c
	@$(CC) $(CFLAGS) -o lab1b-server lab1b-server.c
lab1b-server:
	@$(CC) $(CFLAGS) -o lab1b-server lab1b-server.c

lab1b-client:
	@$(CC) $(CFLAGS) -o lab1b-client lab1b-client.c
clean:
	@rm -f lab1b-server lab1b-client *.tar.gz *.txt
dist: default
	tar -czf $(TARBALL) $(SOURCE)

