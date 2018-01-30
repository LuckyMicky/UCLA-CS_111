/*
NAME:  Daxuan Shu
ID:	   2*******1	
Email: daxuan@g.ucla.edu	  	
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <errno.h> // for errno
#include <string.h> // for strerror
#include <signal.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE  1024
int isShell = 0;
pid_t child_pid = -1;

int parent_to_child[2];
int child_to_parent[2];

/* Use this variable to remember original terminal attributes. */
struct termios saved_attributes;

void reset_terminal_mode (void)
{
  tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
  if (isShell)
  {
    int status = 0;
        if (waitpid(child_pid, &status, 0) == -1) 
        {
            fprintf(stderr, "error: waitpid failed%s\n",strerror(errno));
            exit(1);
        }
        if (WIFEXITED(status)) 
        {
            fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
            exit(0);
        }
  }
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

void handler(int signum) 
{
    if(signum == SIGPIPE)
        exit(0);
}

// two char pointer to deal with <cr> <lf>
char crlf[2] = {0x0D,0x0A};
char lf[1] = {0x0A};

int main (int argc, char **argv)
{
  int ret;
  static struct option long_options[] = 
    {
      {"shell", no_argument, 0, 'S'},
      {0,       0,           0,  0}
    };

  while (1)
  {
    ret = getopt_long (argc, argv, "S", long_options, 0);
    if (ret == -1)
      break;
    switch (ret)
    {
      case 'S':
	      signal(SIGINT, handler);
        signal(SIGPIPE, handler);
        isShell = 1;
        break;
      default:
        fprintf(stderr, "Unrecognized argument\n");
        exit(1);
        break;
    }
  }
  
  set_terminal_mode(); //restore the initial terminal setting.

  if (!isShell)
  {
    char b[BUFFER_SIZE];
    int count = 0;
    while (1) 
    {  // if hasn't read C-d
      count = read(0, b, BUFFER_SIZE);
      for (int i = 0; i < count; i++)
      {
        char c = b[i];
        switch (c) 
        {
          case 0x04:
            exit(0); //Becareful!!!! _exit(0) does not restore the normal terminal cause _exit() is the system call function, which will bypass the atexit() function!
            break;
          case 0x0D:
          case 0x0A:
            write(STDOUT_FILENO, crlf, 2);
            break;
          default :
            write(STDOUT_FILENO, &c, 1);
        }
      
      }

    }
  exit(0);
  }

  
  if (isShell)
  {
    
    
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
    
    if (child_pid > 0) // parent process
    { 
      close(parent_to_child[0]);
      close(child_to_parent[1]);

      struct pollfd pfd[2];
    
      pfd[0].fd=STDIN_FILENO;
      pfd[0].events=POLLIN | POLLHUP | POLLERR;
    
      pfd[1].fd=child_to_parent[0];
      pfd[1].events=POLLIN | POLLHUP | POLLERR;

      for(;;)
      {
	      if (poll(pfd, 2, 0) == -1)
        {
          fprintf(stderr, "error in poll(): %s\n", strerror(errno));
          exit(1);
        }
      
        if (pfd[0].revents & POLLIN)
        {
          char buffer[BUFFER_SIZE];
          int count = read(STDIN_FILENO, buffer, BUFFER_SIZE);
          for (int i = 0; i < count; i++)
          {
            char cur = buffer[i];
            switch (cur)
            {
              case 0x0D:
              case 0x0A:
                write(parent_to_child[1], lf,1);
                write(STDOUT_FILENO, crlf, 2);
                break;
              case 0x03:
                kill(child_pid, SIGINT);
                break;
              case 0x04:
                close(parent_to_child[1]);
                break;
              default:
                write(parent_to_child[1], &cur , 1);
                write(STDOUT_FILENO, &cur, 1);
            }
          }
        }

        if (pfd[0].revents & (POLLHUP + POLLERR))
        {
          fprintf(stderr, "error in shell");
  	      exit(1);
        }

      
        if (pfd[1].revents & POLLIN)
        {
          char buffer[BUFFER_SIZE];
          int count = read(child_to_parent[0], buffer, BUFFER_SIZE);
          for (int j = 0; j < count; j++)
          {
            char cur = buffer[j];
            switch (cur)
            {
              case 0x0A:
                write(STDOUT_FILENO, crlf, 2);
                break;
              default:
                write(STDOUT_FILENO, &cur, 1);
            }
          }
        }

        if (pfd[1].revents & (POLLHUP | POLLERR))
        {
          close(child_to_parent[0]);
          exit(0);
        }
      }
    }

    else if (child_pid == 0) // child process
         { 
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            dup2(parent_to_child[0], STDIN_FILENO);
            dup2(child_to_parent[1], STDOUT_FILENO);
            //dup2(child_to_parent[1], STDERR_FILENO);
            close(parent_to_child[0]);
            close(child_to_parent[1]);

            char *execvp_argv[2];
            char execvp_filename[] = "/bin/bash";
            execvp_argv[0] = execvp_filename;
            execvp_argv[1] = NULL;
            if (execvp("/bin/bash", execvp_argv) == -1) 
            {
              fprintf(stderr, "execvp() failed!%s\n", strerror(errno));
              exit(1);
            }

          }  
          else // fork() failed!
          { 
            fprintf(stderr, "fork() failed!%s\n", strerror(errno) );
            exit(1);
          }
  }
  
  return 0;
}





	
	
