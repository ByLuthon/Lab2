/*
 *
 * Uppgiften gick ut pa att skriva ett program som fungerar
 * som en enkel kommandotolk (shell) för UNIX. 
 * tanken är att  Programmet skall tillåta användaren att mata in 
 * kommandon till dess han/hon väljer att avsluta kommandotolken med kommandot “exit”. 
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


#define MAX_CMD_SIZE 1024
#define MAX_CMD_ARGS 64
#define MAX_BG_PROCS 64

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
#define DEBUG(fmt, ...)

/* Function to read shell command line from the user */
int read_cmd()
{
  char *s;

  memset(cmd_buffer, 0, sizeof(cmd_buffer));
  memset(cmd_args, 0, sizeof(cmd_args));
  cmd_args_count = 0;

  printf(">> ");

  s = fgets(cmd_buffer, sizeof(cmd_buffer), stdin);
  if (s == NULL) {
    return -1;
  }

  /* seperate command line into arguments and store them */
  s = strtok(cmd_buffer, " \n");
  while (s != NULL && cmd_args_count < MAX_CMD_ARGS - 1) {
    cmd_args[cmd_args_count] = s;
    DEBUG("%d: %s", cmd_args_count, s)
    cmd_args_count += 1;
    s = strtok(NULL, " \n");
  }
  cmd_args[cmd_args_count] = NULL;
  return 0;
}

/* change current directory command */
void change_dir()
{
  struct stat st;
  int ret;
  char *dir = getenv("HOME");

  /* when directory argument is specified and is valid
     we change directory to it. Otherwise we change
     directory to HOME environment variable
  */
  if (cmd_args_count > 1) {
    ret = stat(cmd_args[1], &st);
    if (ret == 0 && S_ISDIR(st.st_mode))
      dir = cmd_args[1];
  }

  DEBUG("CHDIR to %s", dir);
  ret = chdir(dir);
  if (ret != 0) {
    printf("Failed to change directory to %s\n", dir);
  }
}

void execute_cmd()
{
  int is_background = 0;
  int i;
  pid_t pid;
  int status;
  int ret;
  struct timeval start, end;
  unsigned long long usec;

  /* detect whether process should be executed in background
     by checking for & argument
  */
  for (i = 0; i < cmd_args_count; i++) {
    if (strcmp(cmd_args[i], "&") == 0) {
      cmd_args[i] = NULL;
      cmd_args_count = i;
      is_background = 1;
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
    printf("Too much background processes currently running\n");
    return;
  }

  gettimeofday(&start, NULL);
  pid = fork();
  if (pid == -1) {
    printf("failed to fork process\n");
    return;
  }

  if (pid == 0) {
    /* restore signal handlers for child */
    signal(SIGINT, SIG_DFL);
#if SIGNAL_DETECTION == 1
    signal(SIGCHLD, SIG_DFL);
#endif
    ret = execvp(cmd_args[0], cmd_args);
    if (ret == -1) {
      printf("Failed to exec %s", cmd_args[0]);
      exit(-1);
    }
  }

  if (!is_background) {
    /* wait for foreground process and report it's
       termination and execution time
    */
    printf("process [%d] executed at foreground\n",
           pid);
    ret = waitpid(pid, &status, 0);
    gettimeofday(&end, NULL);
    usec = 1000000 * (end.tv_sec - start.tv_sec);
    usec += end.tv_usec;
    usec -= start.tv_usec;
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
    if (bg_procs[i] == 0) {
      bg_procs[i] = pid;
      break;
    }
  }
#endif
  bg_procs_num += 1;

}

/* hande cd, exit and execute process commands */
int handle_cmd()
{
  if (cmd_args_count < 1) {
    return 0;
  }

  if (strcmp(cmd_args[0], "exit") == 0) {
    exit(0);
  }
  else if (strcmp(cmd_args[0], "cd") == 0) {
    change_dir();
  }
  else {
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
  if (bg_procs_num > 0) {
    for (i = 0; i < MAX_BG_PROCS; i++) {
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
        bg_procs[i] = 0;
        bg_procs_num -= 1;
      }
    }
  }

}

#if SIGNAL_DETECTION == 1
void child_event(int sig)
{
  pid_t pid;
  int   status;
  int i = 0;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    DEBUG("event pid=%d, sig=%d", (int)pid, sig);
    while (i < MAX_BG_PROCS) {
      if (bg_procs[i] == 0) {
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
  signal(SIGINT, SIG_IGN);
#if SIGNAL_DETECTION == 1
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

  memset(bg_procs, 0, sizeof(bg_procs));
  bg_procs_num = 0;

  ret = setup_signals();
  if (ret != 0) {
    printf("Failed to seupt signals handling\n");
    return -1;
  }

  while (1) {
    ret = read_cmd();
    detect_terminated_procs();

    if (ret != 0) {
      printf("Failed to read cmd\n");
      continue;
    }

    ret = handle_cmd();
    if (ret != 0) {
      printf("failed to handle cmd [%s]\n", cmd_args[0]);
      continue;
    }

  }
  return 0;
}
