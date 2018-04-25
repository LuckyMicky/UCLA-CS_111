/*
NAME:  Daxuan Shu
ID:    2*******1    
Email: daxuan@g.ucla.edu        
*/

// lab1b_server.c

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <poll.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "zlib.h"

#define BUFFER_SIZE  1024
#define COMSIZE 2048

pid_t child_pid = -1;

char crlf[2] = {0x0D,0x0A};
char lf[1] = {0x0A};

int parent_to_child[2];
int child_to_parent[2];
int sockfd, newsockfd, portno;
uint clilen;
struct pollfd pfd[2];
struct sockaddr_in serv_addr, cli_addr;

int port_flag, compress_flag;
int count = 0;

z_stream client_to_server;
z_stream server_to_client;

unsigned char com_buf[BUFFER_SIZE]; //compression buffer
unsigned char buffer[BUFFER_SIZE];
unsigned char socket_buffer[BUFFER_SIZE];

void shell_exit_status()
{
	int status = 0;
        if (waitpid(child_pid, &status, 0) == -1) // waitpid(): on success, returns the process ID of the child whose state has changed
                                                  // On error, -1 is returned.
        {
            fprintf(stderr, "error: waitpid failed%s\n",strerror(errno));
            exit(1);
        }
        if (WIFEXITED(status)) // WIFEXITED(status) returns true if the child terminated normally by calling exit(3)
        {
            /*
                WTERMSIG(status): returns the number of the signal that caused the child process to terminate. 
                WEXITSTATUS(status): returns the exit status of the child.
            */
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status)); 
            exit(0);
        }
}


void handler(int signum)  // don't use in multithreading  
{
    if(signum == SIGPIPE)
        exit(1);
}

void end_compression()
{
    inflateEnd(&server_to_client);
    deflateEnd(&client_to_server);
}

int main (int argc, char **argv)
{
	int ret;
  struct hostent *server;
    static struct option long_options[] = 
    {
      {"port", required_argument, 0, 'P'},
      {"compress", no_argument, 0, 'C'},
      {0,       0,           0,  0}
    };

    signal(SIGPIPE, handler);
    atexit(shell_exit_status);
    
    while (1)
    {
        ret = getopt_long (argc, argv, "P:", long_options, 0);
        if (ret == -1)
          break;
        switch (ret)
        {
          case 'P':
            port_flag = 1;
            portno = atoi(optarg);
            /* First call to socket() function */
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) 
            {
              fprintf(stderr, "ERROR opening socket%s\n", strerror(errno));
              exit(1);
            }
            /* Initialize socket structure */
            server = gethostbyname("localhost");
            memset((char *) &serv_addr, 0, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_addr.s_addr = INADDR_ANY;
            serv_addr.sin_port = htons(portno);
            memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
            
            /* Now bind the host address using bind() call.*/
            if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
            {
              fprintf(stderr, "ERROR on binding%s\n", strerror(errno));
              exit(1);
            }
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

  if(compress_flag)
  {
    /* Initialize internal stream state for compression & decompression */
    client_to_server.zalloc = Z_NULL;
    client_to_server.zfree = Z_NULL;
    client_to_server.opaque = Z_NULL;

    if(deflateInit(&client_to_server, Z_DEFAULT_COMPRESSION) != Z_OK)
        {
            fprintf(stderr, "error: --compress: deflateInit() failed: %s\n", client_to_server.msg);
            exit(1);
        }

    atexit (end_compression);   
  }

  /* Now start listening for the clients, here process will
      * go in sleep mode and will wait for the incoming connection
   */
   
   listen(sockfd,5);
   clilen = sizeof(cli_addr);

   /* Accept actual connection from the client */
   newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
   if (newsockfd < 0) 
   {
      fprintf(stderr, "ERROR on accept%s\n", strerror(errno));
      exit(1);
   }

   /* If connection is established then start communicating */ //!!

    if (pipe(parent_to_child) == -1) 
    {
      fprintf(stderr, "pipe() failed!%s\n", strerror(errno));
      exit(1);
    }

    if (pipe(child_to_parent) == -1) 
    {
      fprintf(stderr, "pipe() failed!%s\n", strerror(errno));
      exit(1);
    }

    child_pid = fork();

    if (child_pid < 0)  // fork() failed
    {  
        fprintf(stderr, "error: fork() failed: %s", strerror(errno));
        exit(1);
    }

    if (child_pid > 0) //parent process
    {
      close(parent_to_child[0]);
      close(child_to_parent[1]);

      struct pollfd pfd[2];
    
      pfd[0].fd = newsockfd;
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
        if (pfd[0].revents & POLLIN)
        {
            count = read(newsockfd, buffer, BUFFER_SIZE);
            if (count < 0)
            {
              fprintf(stderr, "error: read() failed: %s\n", strerror(errno));
              exit(1);
            }  

            if (compress_flag)
            {
              server_to_client.avail_in = count;
              server_to_client.next_in = buffer;
              server_to_client.avail_out = BUFFER_SIZE;
              server_to_client.next_out = com_buf;

              do
              {
                inflate(&server_to_client,Z_SYNC_FLUSH);
              } while(server_to_client.avail_in > 0);
              count = BUFFER_SIZE - server_to_client.avail_out;
              memcpy(buffer, com_buf, count);  
            }

              for (int i = 0; i < count; i++)
              {
                char cur = buffer[i];
                switch (cur)
                {
                  case 0x0A:
                  case 0x0D:
                    if (write(parent_to_child[1], lf, 1) < 0)
                    {
                      fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                      exit(1);
                    }  
                    break;
                  case 0x04:
                    close(parent_to_child[1]);
                    break;
                  case 0x03:
                    kill(child_pid, SIGINT);
                    break;
                  default:
                  if (write(parent_to_child[1], &cur , 1) < 0)
                  {
                    fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                    exit(1);
                  }
                  break;
                }
              }
        }  
        
        if (pfd[1].revents & POLLIN)
        {
          count = read(child_to_parent[0], buffer, BUFFER_SIZE);
          if (count < 0)
          {
            fprintf(stderr, "error: read() failed: %s\n", strerror(errno));
            exit(1);
          }

          if(compress_flag)
          {
            client_to_server.avail_in = count;
            client_to_server.next_in = buffer;
            client_to_server.avail_out = BUFFER_SIZE;
            client_to_server.next_out = com_buf;

            do
            {
              deflate(&client_to_server,Z_SYNC_FLUSH);
            } while(client_to_server.avail_in > 0);

            count = BUFFER_SIZE - client_to_server.avail_out;
            memcpy(buffer, com_buf, count);
           }   
            for (int i = 0; i < count; i++)
              {
                char cur = buffer[i];
                switch (cur)
                {
                  default:
                  if (write(newsockfd, &cur , 1) < 0)
                  {
                    fprintf(stderr, "error: write() failed: %s\n", strerror(errno));
                    exit(1);
                  }
                  break;
                }
              }
        }
         if (pfd[1].revents & (POLLHUP + POLLERR))
        {
          close(child_to_parent[0]);          
          exit(0);
        }

        }
      }

      if (child_pid == 0)
      {
        close(parent_to_child[1]);
        close(child_to_parent[0]);
        dup2(parent_to_child[0], STDIN_FILENO);
        dup2(child_to_parent[1], STDOUT_FILENO);
        dup2(child_to_parent[1], STDERR_FILENO);
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        char *execvp_argv[2];
        char execvp_filename[] = "/bin/bash";
        execvp_argv[0] = execvp_filename;
        execvp_argv[1] = NULL;
        if (execvp("/bin/bash", execvp_argv) == -1) 
        {
          fprintf(stderr, "execvp() failed!%s\n", strerror(errno));
          close(sockfd);
          close(newsockfd);
          exit(1);
        }
        exit(0);
      }

      return 0;
}
