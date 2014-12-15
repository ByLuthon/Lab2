/*
 *
 * Uppgiften gick ut pa att skriva ett program som fungerar
 * som en enkel kommandotolk (shell) för UNIX. 
 * tanken är att  Programmet skall tillåta användaren att mata in 
 * kommandon till dess han/hon väljer att avsluta kommandotolken med kommandot “exit”. 
 *
 * Created by Luthon Hagvinprice on 12/15/14.
 *kone@kth.se
 *Student på ICT KTH Kista Information an kommunikation technology
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>

/* command entered must be at most 1024 characters */
#define MAX_CMD_SIZE 1024
/* there are at most 64 words seperated by spaces in the command which was entered */
#define MAX_CMD_ARGS 64
/* there shell can run at most 64 background processes */
#define MAX_BG_PROCS 64

/* macro used to determine how we detect terminated background processes */
#ifndef SIGNAL_DETECTION
/* when this macro is set to 1 we use signal handling to detect child
   termination
   when it is set to 0 we use polling of child processes to detect
   child termination
*/
#define SIGNAL_DETECTION 1
#endif

/* buffer to read command line */
char cmd_buffer[MAX_CMD_SIZE];
/* buffer to store command arguments before we
   execute the process using execvp
*/
char *cmd_args[MAX_CMD_ARGS];
/* num of command arguments we store */
int cmd_args_count;

/* pids of background processes
   When SIGNAL_DETECTION enabled we use this array
   to store pids of proceses which were terminated

   When SIGNAL_DETECTION is disable we store here
   pids of all executed processes.
*/
pid_t bg_procs[MAX_BG_PROCS];
/* number of executed processes */
int bg_procs_num;

/* #define DEBUG(fmt, ...) printf("DEBUG: " fmt "\n", ##__VA_ARGS__) */
/* discard debug printouts */
#define DEBUG(fmt, ...)

/* Function to read shell command line from the user */
int read_cmd()
{
  char *s;

  /* zero cmd_buffer structure */
  memset(cmd_buffer, 0, sizeof(cmd_buffer));
  /* zero cmd_args structure */
  memset(cmd_args, 0, sizeof(cmd_args));
  cmd_args_count = 0;

  /* print command line */
  printf(">> ");

  /* read line from the standard input */
  s = fgets(cmd_buffer, sizeof(cmd_buffer), stdin);
  if (s == NULL) {
    /* return on failure */
    return -1;
  }

  /* seperate command line into arguments and store them */
  s = strtok(cmd_buffer, " \n");
  while (s != NULL && cmd_args_count < MAX_CMD_ARGS - 1) {
    /* for each token argument store it in cmd_args array */
    cmd_args[cmd_args_count] = s;
    DEBUG("%d: %s", cmd_args_count, s)
    cmd_args_count += 1;
    /* continue to the next word */
    s = strtok(NULL, " \n");
  }
  /* NULL specifies end of arguments
     this will be usefull later when we call execvp
  */
  cmd_args[cmd_args_count] = NULL;
  return 0;
}

/* change current directory command */
void change_dir()
{
  /* store stat info about the file */
  struct stat st;
  int ret;
  /* read environment variable HOME */
  char *dir = getenv("HOME");

  /* when directory argument is specified and is valid
     we change directory to it. Otherwise we change
     directory to HOME environment variable
  */
  if (cmd_args_count > 1) {
    /* when there is at least one argument try to check
       if this is a valid path
       otherwise the environment variable HOME will be used
    */
    ret = stat(cmd_args[1], &st);
    /* check that the file is a valid directory name */
    if (ret == 0 && S_ISDIR(st.st_mode))
      dir = cmd_args[1];
  }

  DEBUG("CHDIR to %s", dir);
  /* use chdir to change directory */
  ret = chdir(dir);
  if (ret != 0) {
    printf("Failed to change directory to %s\n", dir);
  }
}

void execute_cmd()
{
  /* flag to specify we are running background process */
  int is_background = 0;
  int i;
  /* pid of the process */
  pid_t pid;
  int status;
  int ret;
  /* start and end time of foreground process execution */
  struct timeval start, end;
  /* number of microseconds passed between process start and end */
  unsigned long long usec;

  /* detect whether process should be executed in background
     by checking for & argument
  */
  for (i = 0; i < cmd_args_count; i++) {
    if (strcmp(cmd_args[i], "&") == 0) {
      /* when argument matches & mark this execution as background
         execution and update number of arguments */
      cmd_args[i] = NULL;
      cmd_args_count = i;
      is_background = 1;
      /* break for loop since we want to find at least one & */
      break;
    }
  }

  /* don't execute commands with single & or commands
     starting with &
  */
  if (cmd_args_count == 0)
    return;

  /* we limit number of background processes to execute
     to MAX_BG_PROCS
  */
  if (is_background && bg_procs_num >= MAX_BG_PROCS) {
    /* prevent from running too much background processes */
    printf("Too much background processes currently running\n");
    return;
  }

  /* record start time of foreground process */
  gettimeofday(&start, NULL);
  /* fork process and try to execute it */
  pid = fork();
  if (pid == -1) {
    /* we failed to fork this is bad news */
    printf("failed to fork process\n");
    return;
  }

  if (pid == 0) {
    /* we are at the child process */
    /* restore signal handlers for child */
    signal(SIGINT, SIG_DFL);
#if SIGNAL_DETECTION == 1
    signal(SIGCHLD, SIG_DFL);
#endif
    /* execute the command using cmd_args */
    ret = execvp(cmd_args[0], cmd_args);
    if (ret == -1) {
      printf("Failed to exec %s", cmd_args[0]);
      exit(-1);
    }
  }

  /* here we at parent process */
  if (!is_background) {
    /* wait for foreground process and report it's
       termination and execution time
    */
    printf("process [%d] executed at foreground\n",
           pid);
    /* wait for child process */
    ret = waitpid(pid, &status, 0);
    /* record process end time */
    gettimeofday(&end, NULL);
    /* compute time difference in microseconds */
    usec = 1000000 * (end.tv_sec - start.tv_sec);
    usec += end.tv_usec;
    usec -= start.tv_usec;
    /* print time in seconds and the fraction in microseconds of the
       command run time
    */
    printf("process [%d] terminated after %lld.%06llds\n",
           (int)pid, usec / 1000000, usec % 1000000);
    return;
  }

  /* background process */
  printf("process [%d] executed at background\n", pid);
#if SIGNAL_DETECTION == 0
  /* when signal detection is not activated
     add pid to array of pids to monitor for
     termination
  */
  for (i = 0; i < MAX_BG_PROCS; i++) {
    /* find empty slot in bg_procs and store the pid
       there. IT will be used later to call waitpid on the pid
    */
    if (bg_procs[i] == 0) {
      bg_procs[i] = pid;
      /* break from the loop we found empty slot */
      break;
    }
  }
#endif
  /* increase number of processes */
  bg_procs_num += 1;

}

/* hande cd, exit and execute process commands */
int handle_cmd()
{
  /* ignore empty commands */
  if (cmd_args_count < 1) {
    return 0;
  }

  if (strcmp(cmd_args[0], "exit") == 0) {
    /* exit shell when command is exit */
    exit(0);
  }
  else if (strcmp(cmd_args[0], "cd") == 0) {
    /* change directory when commands is cd */
    change_dir();
  }
  else {
    /* otherwise try to execute process */
    execute_cmd();
  }
  return 0;
}

/* detect background processes which were terminated */
void detect_terminated_procs()
{
  int i;
#if SIGNAL_DETECTION == 0
  int ret;
  int status;
#endif
  /* make sure you have at least one background process */
  if (bg_procs_num > 0) {
    for (i = 0; i < MAX_BG_PROCS; i++) {
      /* find slot with pid != 0 */
      if (bg_procs[i] == 0)
        continue;

#if SIGNAL_DETECTION == 0
      /* if signal detection disable execute waitpid
         to test if the process was terminated
      */
      ret = waitpid(bg_procs[i], &status, WNOHANG);
      if (ret > 0) {
#else
      if (1) {
        /* if signal detection enabled wait call was already performed
           for process
        */

#endif
        printf("background process [%d] terminated\n",
               (int)bg_procs[i]);
        /* reset process slot and decrease number of background processes */
        bg_procs[i] = 0;
        bg_procs_num -= 1;
      }
    }
  }

}

#if SIGNAL_DETECTION == 1
/* signal handler for SIGCHLD signal */
void child_event(int sig)
{
  pid_t pid;
  int   status;
  int i = 0;
  /* in loop find next child which was terminated */
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    DEBUG("event pid=%d, sig=%d", (int)pid, sig);
    while (i < MAX_BG_PROCS) {
      if (bg_procs[i] == 0) {
        /* find new empty slot in bg_procs and record the terminated pid inside */
        bg_procs[i] = pid;
        i += 1;
        break;
      }
      i += 1;
    }
  }
}
#endif

int setup_signals()
{
  /* ignore Ctrl-c signals from children */
  signal(SIGINT, SIG_IGN);
#if SIGNAL_DETECTION == 1
  /* receive signals from children when they are terminated */
  signal(SIGCHLD, child_event);
  printf("signal detection enabled\n");
#else
  printf("signal detection disabled\n");
#endif
  return 0;
}

int main(int argc, char *argv[])
{
  int ret;

  /* initialize bg_procs to 0 */
  memset(bg_procs, 0, sizeof(bg_procs));
  bg_procs_num = 0;

  /* setup signal handlers */
  ret = setup_signals();
  if (ret != 0) {
    printf("Failed to seupt signals handling\n");
    return -1;
  }

  /* repeat until exit command entered */
  while (1) {
    /* read new command from user */
    ret = read_cmd();
    /* detect terminated background processes and report them */
    detect_terminated_procs();

    if (ret != 0) {
      /* if we failed the cmd read command again */
      printf("Failed to read cmd\n");
      continue;
    }

    /* try to process the command */
    ret = handle_cmd();
    if (ret != 0) {
      printf("failed to handle cmd [%s]\n", cmd_args[0]);
      continue;
    }

  }
  return 0;
}
