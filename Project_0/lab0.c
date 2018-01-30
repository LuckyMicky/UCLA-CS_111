/*
Name:  Daxuan Shu
ID:   204853061
Email: daxuan@g.ucla.edu
*/
#include <getopt.h> // for getopt_long(3)
#include <stdio.h> // for fprintf(3)
#include <unistd.h> // for exit(2), read(2), close(2), dup(2), read(2), write(2)
#include <string.h> // for strerror(3)
#include <signal.h> // for signal(2)
#include <stdlib.h> // for exit(2), malloc, free
#include <errno.h> // for errno
/* The folloing three header files for open(2), creat(2)*/
#include <sys/types.h> 
#include <sys/stat.h>  
#include <fcntl.h>    


// Signal Handler Function
void handler(int signum) {
  fprintf(stderr, "--catch segment fault. Signal number: %d\n%s%s\n", signum,"catch signal: ", strerror(errno));
  // It is not safe to use fprintf inside the signal handler function (details see async-signal-safe and malloc())
    _exit(4);
}

int main(int argc, char **argv)
{
  int ret;
  char* input_arg = NULL;
  char* output_arg = NULL;
  int Segfault = 0;
  char* n = NULL;
  while (1)
    {
     static struct option long_options[] =
        {
          
          /* These options don’t set a flag.
             We distinguish them by their indices. */
          {"input",    required_argument, 0, 'I'},
          {"output",   required_argument, 0, 'O'},
          {"segfault", no_argument,       0, 'S'},
          {"catch",    no_argument,       0, 'C'},
	  {0,          0,                 0,  0} // tell the c what is the size of the struct?
        };
 
     ret = getopt_long (argc, argv, "i:o:sc", long_options, 0);

     /* Detect the end of the options. */
     if (ret == -1)
        break;

     switch (ret)
       {
          case 'I':
	    input_arg = optarg;
	    break;
          case 'O':
	    output_arg = optarg;
	    break;
          case 'S':
	    Segfault = 1;
	    break;
          case 'C':
	    signal(SIGSEGV, handler); // Here, handler's type is sighandler_t which is a type
	                              // of handler functions
	    break;
          default:
	    fprintf(stderr, "Error: unrecognized argument\n");
	    _exit(1);
	    break;
       }
    }
  int ifd = 0;
  int ofd = 1;
  if (input_arg)
    {
      ifd = open(input_arg, O_RDONLY);
      if (ifd >= 0)
	{
	  close(0); // dup2(can slightly close the file descriptor before reused, Thus actually do not need close(0) here! Be careful !)
	  dup2(ifd,0); // Using dup2() is better under multithreading environment
	  //dup(ifd);
	  close(ifd);
	}
      else
	{
	  fprintf(stderr, "Error: --input\nUnable to open the input file: %s\n%s\n", input_arg, strerror(errno));
	  _exit(2);
	}
    }
  if (output_arg)
    {
      ofd = creat(output_arg, S_IRWXU);
      if (ofd >= 0)
	{
	  close(1);
	  dup2(ofd,1);
	  //dup(ofd);
	  close(ofd); 
	}
      else
	{
	  fprintf(stderr, "Error: --output\nUnable to create the outputfile: %s\n%s\n", output_arg, strerror(errno));
	  _exit(3);
	}
    }
  if (Segfault)
    {
      *n = 'A';
    }
  
  char* buffer = (char*) malloc(sizeof(char)); // The size of the pointer to char type is sizeof (int*) bytes.
                                         // The value is implementation defined but is usually 4 bytes
                                         // in 32-bit system and 8 bytes in 64-bit system.
                                         // Here, using malloc to allocate exactly 1 bytes to buffer.
  ssize_t rv = read(0, buffer, 1);
  while(rv > 0)
    {
      write(1, buffer, 1);
      rv = read(0, buffer, 1);
    }
  free(buffer);
  exit(0);
}
