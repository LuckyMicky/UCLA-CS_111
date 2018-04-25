/*
NAME:  Daxuan Shu
ID:	   2*******1	
Email: daxuan@g.ucla.edu	  	
*/

// lab1b_client.c

#include <string.h> // strerror(3)
#include <stdlib.h>
#include <stdio.h>
#include <errno.h> // for errno
#include <sys/socket.h> // for socket(7)
#include <netdb.h> // for gethostbyname(3)
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "zlib.h"
#include <fcntl.h>

#define BUFFER_SIZE  1024
#define COMSIZE 2048

struct termios saved_attributes;
pid_t child_pid = -1;
int is_log = 0;
int sockfd, logfd;

z_stream stdin_to_shell;
z_stream shell_to_stdout;

int port_flag, log_flag, compress_flag;
char* log_file;

char crlf[2] = {0x0D,0x0A};
char lf[1] = {0x0A};

int parent_to_child[2];
int child_to_parent[2];

unsigned char com_buf[BUFFER_SIZE]; //compression buffer
unsigned char dec_buf[BUFFER_SIZE]; //decompression buffer
unsigned char keyboard_buffer[BUFFER_SIZE];
unsigned char buffer[BUFFER_SIZE];
unsigned char socket_buffer[BUFFER_SIZE];
char log_buf[BUFFER_SIZE];

struct pollfd pfd[2];
struct sockaddr_in serv_addr;

void reset_terminal_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes); 
}

void set_terminal_mode (void)
{
  struct termios tattr;
  /* Save the terminal attributes so we can restore them later. */
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit (reset_terminal_mode); // restore the normal terminal setting before exit the program.

  /* Set the funny terminal modes. */
  tcgetattr (STDIN_FILENO, &tattr);
  // tattr.c_cc[VMIN] = 1;  // for UCLA seasnet server, the default value of VMIN = 1.
  // tattr.c_cc[VTIME] = 0;  // for UCLA seasnet server, the default value of VTIME = 0.
  tattr.c_iflag = ISTRIP;	/* only lower 7 bits	*/
  tattr.c_oflag = 0;		/* no processing	*/
  tattr.c_lflag = 0;	
  tcsetattr (STDIN_FILENO, TCSANOW, &tattr);
}

void end_compression()
{
    inflateEnd(&shell_to_stdout);
    deflateEnd(&stdin_to_shell);
}

int main (int argc, char **argv)
{
  int ret, portno;
  struct hostent *server;
  int count = 0;
  int log_count = 0;

  static struct option long_options[] = 
    {
      {"port", required_argument, 0, 'P'},
      {"log", required_argument, 0, 'L'},
      {"compress", no_argument, 0, 'C'},
      {0,       0,           0,  0}
    };

  while (1)
  {
    ret = getopt_long (argc, argv, " ", long_options, 0);
    if (ret == -1)
      break;
    switch (ret)
    {
      case 'P':
		    portno = atoi(optarg);
        port_flag = 1;
        /* Create a socket point */
        sockfd = socket(AF_INET, SOCK_STREAM, 0); /* AF_INET: IPv4 Internet protocols
                                                SOCK_STREAM: Provides sequenced, reliable, two-way, connection-
                                                based byte streams.  An out-of-band data transmission
                                                mechanism may be supported.*/
        if (sockfd < 0) 
        {
          fprintf(stderr, "Error: Unable to open socket.%s\n", strerror(errno));
          exit(1);
        }
        server = gethostbyname("localhost"); //gethostbyname() is obsolete, find a better function if had time.
        // bzero and bcopy are obsolete. Use memset(3) and memcpy(3) instead.
        memset((char *) &serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
        serv_addr.sin_port = htons(portno);
         
         /* Now connect to the server */
        if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) 
        {
          fprintf(stderr, "Error: Unable to connect.%s\n", strerror(errno));
          exit(1);
        }
        break;

      case 'L':
        log_file = optarg;
        logfd = creat(log_file, 0644);
      	if (logfd < 0)
        {
      		fprintf(stderr, "Error: Unable to create log file.%s\n", strerror(errno));
          exit(1);
        }
        log_flag = 1;
      	break;
      case 'C':
        compress_flag = 1;
        break;
      default:
        fprintf(stderr, "Unrecognized argument\n");
        exit(1);
        break;
    }
  }

  if (port_flag == 0)
  {
     fprintf(stderr, "Port switch is mandatory. --port=port#\n");
     exit(1);
  }
  
  set_terminal_mode(); //restore the initial terminal setting.

 if (compress_flag)
      {
        stdin_to_shell.zalloc = Z_NULL;
        stdin_to_shell.zfree=Z_NULL;
        stdin_to_shell.opaque = Z_NULL;

        //Initializes the internal stream state for compression
        if (deflateInit(&stdin_to_shell, Z_DEFAULT_COMPRESSION) != Z_OK)
        {
          fprintf(stderr, "Unable to initialize stream state for compression %s\n",strerror(errno));
          exit(1);
        }

        shell_to_stdout.zalloc = Z_NULL;
        shell_to_stdout.zfree = Z_NULL;
        shell_to_stdout.opaque = Z_NULL;

        if (inflateInit(&shell_to_stdout) != Z_OK)
        {
          fprintf(stderr, "unable to initialze stream state for decompression %s\n",strerror(errno));
          exit(1);
        }

        atexit(end_compression);
      }

  pfd[0].fd = STDIN_FILENO;  
  pfd[0].events = POLLIN | POLLHUP | POLLERR;
    
  pfd[1].fd = child_to_parent[0];
  pfd[1].events = POLLIN | POLLHUP | POLLERR;


  while(1)
  {
    if (poll(pfd, 2, 0) == -1)
    {
      fprintf(stderr, "error in poll(): %s\n", strerror(errno));
      exit(1);
    }

    if (pfd[0].revents & POLLIN) // when there is waiting input
    {
      count = read(STDIN_FILENO, keyboard_buffer, BUFFER_SIZE);
      if(count < 0)
            {
                fprintf(stderr, "error: read() failed: %s\n", strerror(errno));
                exit(1);
            }
      if (compress_flag)
      {  
        stdin_to_shell.avail_out = BUFFER_SIZE;
        stdin_to_shell.next_out = com_buf;
        stdin_to_shell.avail_in = count;
        stdin_to_shell.next_in = keyboard_buffer;
        
        do
        {
          deflate(&stdin_to_shell, Z_SYNC_FLUSH);
        }while(stdin_to_shell.avail_in > 0);
      }

      for (int i = 0; i < count; i++)
      {
        char cur = buffer[i];
        switch (cur)
        {
          case 0x0D:
          case 0x0A:
            if(write(STDOUT_FILENO, crlf, 2) < 0)
            {
              fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                        exit(1);
            }
          break;
          default:
            if (write(STDOUT_FILENO, &cur, 1) < 0)
            {
              fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                        exit(1);
            }
          break;
        }
        if(write(sockfd, &cur, 1) < 0)
                {
                    fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                    exit(1);
                }
      }  
      if (log_flag)
      {
        log_count = sprintf(log_buf, "SENT %d bytes: ", count);
        if (write(logfd, log_buf, log_count) < 0)
        {
          fprintf(stderr, "Error writing to logfile %s\n", strerror(errno));
          exit(1);
        }
        if (write(logfd, buffer, log_count) < 0)
        {
          fprintf(stderr, "Error writing to logfile %s\n", strerror(errno));
          exit(1);
        }
        if(write(logfd, "\n" , 1) < 0)
        {
          fprintf(stderr, "error: --log(): write() failed: %s\n", strerror(errno));
          exit(1);
        }
      }  
    }

    if (pfd[1].revents & POLLIN)
    {
      count = read(sockfd, socket_buffer, BUFFER_SIZE);
      if (count < 0)
      {
        fprintf(stderr, "Unable to read from socket %s\n", strerror(errno));
        exit(1);
      }

      if (count == 0)
      {
        exit(0);
      }

      if (log_flag)
        {
          log_count = sprintf(log_buf, "RECEIVED %d bytes: ", count);
          if (write(logfd, log_buf, log_count) < 0)
          {
            fprintf(stderr, "error: --log(): write() failed: %s\n", strerror(errno));
                    exit(1);
          } 
          if(write(logfd, buffer, count) < 0)
          {   
            fprintf(stderr, "error: --log(): write() failed: %s\n", strerror(errno));
            exit(1);
          }
          if(write(logfd, "\n" , 1) < 0)
          {
            fprintf(stderr, "error: --log(): write() failed: %s\n", strerror(errno));
            exit(1);
          }
        }
      if (compress_flag)
      {
        shell_to_stdout.avail_out = BUFFER_SIZE;
        shell_to_stdout.next_out = com_buf;
        shell_to_stdout.avail_in = count;
        shell_to_stdout.next_in = buffer;

        do
        {
          inflate(&shell_to_stdout,Z_SYNC_FLUSH);
        }while(shell_to_stdout.avail_in>0);

        count = BUFFER_SIZE - shell_to_stdout.avail_out;
        memcpy(buffer, com_buf, count);
      } 

      for (int i = 0; i < count; i++)
      {
        char cur = buffer[i];
        switch (cur)
        {
          case 0x0A:
            if(write(STDOUT_FILENO, crlf, 2) < 0)
            {
              fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                        exit(1);
            }
          break;
          default:
            if (write(STDOUT_FILENO, &cur, 1) < 0)
            {
              fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
              exit(1);
            }
          break;
        }
      } 
    }

    if (pfd[0].revents & (POLLHUP + POLLERR))
    {
     exit(0);
    }
    
    if (pfd[1].revents & (POLLHUP | POLLERR))
    {
      exit(0);
    }


  }
  return 0; //while(1)
}// int main()
