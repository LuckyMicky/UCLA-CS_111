# NAME:  Daxuan Shu
# UID:   204853061
# Email: daxuan@g.ucla.edu

CC = gcc
CFLAGS = -Wall -Wextra -g
UID = 204853061
TARBALL = lab0-$(UID).tar.gz
SOURCE = lab0.c Makefile README breakpoint.png backtrace.png

default:
	@$(CC) $(CFLAGS) -o lab0 lab0.c
check:  clean default smoke-tests
	@echo "All smoke-test cases passed.";
clean:
	@rm -f *.o lab0 *.txt *.tar.gz
dist: default
	tar -czf $(TARBALL) $(SOURCE)
# Smoke-tests cases to check if the program works as it should be. They are prerequisites for check.
smoke-tests: Input Output Catch
Input:
	@echo "Hello Daxuan" > testin.txt; \
	./lab0 --input=testin.txt --output=testo.txt; \
	if [ $$? -ne 0 ]; then \
	  echo "Input test failed"; \
	  exit 1; \
	fi
	@rm -f testo.txt; \
	./lab0 --input=null.txt --output=testo.txt > out 2>&1 /dev/null; \
	if [ $$? -ne 2 ]; then \
	  echo "Input test failed: open an unexistent file."; \
	  exit 1; \
	fi
	@chmod -r testin.txt; \
	./lab0 --input=testin.txt --output=testo.txt > out 2>&1 /dev/null; \
	if [ $$? -ne 2 ]; then \
	  echo "Input test failed: open an input file without permission."; \
	  exit 1; \
	fi
	@rm -f testin.txt;
Output:
	@echo "Mimi" > testin.txt; \
	touch testo.txt; \
	chmod -w testo.txt; \
	./lab0 --input=testin.txt --output=testo.txt > out 2>&1 /dev/null; \
	if [ $$? -ne 3 ]; then \
	  echo "Output test failed: Write to an output file without permission."; \
	  exit 1; \
	fi
	@rm -f testin.txt testo.txt;
Catch:
	@./lab0 --segfault --catch > out 2>&1 /dev/null; \
	if [ $$? -ne 4 ]; then \
	  echo "Catch test failed: Return value for catch should be 4."; \
	  exit 1; \
	fi

