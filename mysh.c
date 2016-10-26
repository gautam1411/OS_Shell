/********************************************************************************************
*  Description: Shell with Job Control 
*  Date       :
*  Version    :
*  Author     : Gautam Singh (gautamsingh@cs.wisc.edu)
*********************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <time.h>

#define BUFFER_SIZE          516
#define BUFFER_SIZE_MAX      513
#define ERROR_MSG_SIZE       1024
#define MAX_BACKGROUND_JOBS  256

#define SHELL_PROMPT         "mysh> "
#define INVALID_ARGUMENTS    "Usage: mysh [batchFile]\n"
#define BATCH_FILE_OPEN_FAIL "Error: Cannot open file %s\n"
#define COMMAND_ERROR        "%s: Command not found\n"
#define INVALID_JID          "Invalid jid %d\n"
#define JID_WAIT_TIME        "%d : Job %d terminated\n"
#define DEBUG                 0

typedef struct JobStruct {
  int jid;
  int job_status;
  int is_free_entry;
  int wait_time;
  pid_t pid;
  char *command[516];
  char command_ip[516];
}JobInfo;

typedef struct JobInfoParamStruct {
  JobInfo *Job;
  int *jid;
  int * is_bg_job;
  char * command_ip;
  pid_t pid;
}JobInfoParam;

void ErrorMessage(int line, const char *file) {
  char number[22];
  char error_message[ERROR_MSG_SIZE] = "Error : file, ";
  strncat(error_message, file, strlen(file));
  strcat(error_message, " at line ");
  sprintf(number, "%d ", line);
  strncat(error_message, number, strlen(number));
  strcat(error_message, "\n");
  write(STDERR_FILENO, error_message, strlen(error_message));
}

char * TrimWhitespace(char *str) {
  char *str_ptr = str;
  char *last;
  while (isspace(*str_ptr) )
    str_ptr++;
  if (0 == (*str_ptr)) {
    str_ptr[0] = 0;
    return  str_ptr;
  }
  last = str_ptr + strlen(str_ptr)-1;
  while (last > str_ptr && isspace(*last) )
  last--;
  last[1] = 0;
  return str_ptr;
}

void PrintShellPrompt() {
  char ShellPrompt[] = SHELL_PROMPT;
  write(STDOUT_FILENO, ShellPrompt, strlen(ShellPrompt));
}

char temp_str[32] = "&";
void ParseShellCommand(char *command_buf, char ** command) {
  char delimeter[] = " ";
  char delimeter2[] = "\t";
  char * last_token = NULL;
  int loop = 0;
  char *token = strtok(command_buf, delimeter);
  if (token == NULL)
    token = strtok(NULL, delimeter2);
  while (NULL != token) {
    command[loop++] = token;
    token = strtok(NULL, delimeter);
    if (token == NULL)
      token = strtok(NULL, delimeter2);
    // while( token && isspace( *token))
    // token++
  }
  command[loop] = NULL;
  // Error Handling for incorrect input
  if (DEBUG) {
    int temp = 0;
    while (command[temp ])
    printf("%s\n", command[temp++]);
    fflush(stdout);
  }
  if (loop > 0) {
     last_token = command[loop-1];
     int len_last_token = strlen(last_token);
     if (len_last_token > 1 && last_token[len_last_token-1] == '&') {
       last_token[len_last_token-1] = 0;
     command[loop] = temp_str;
     command[loop+1] = NULL;
     }
  }
  if (DEBUG) {
      int temp = 0;
      while (command[temp])
      printf("%s\n", command[temp++]);
      fflush(stdout);
  }
}

char temp_ws[20] = " ";
void AddBackgroundJob(char ** command, JobInfoParam *job_info_param) {
  JobInfo *Job = job_info_param->Job;
  int *jid = job_info_param->jid;
  char * cmd_ip_ptr = job_info_param->command_ip;
  int loop = 0;
  char *pos;
  int l;
  int com[1024];

  loop = 0;
  for (; loop < MAX_BACKGROUND_JOBS; loop++) {
    if (0 == Job[loop].jid) {
      Job[loop].is_free_entry = 1;
      Job[loop].jid = *jid;
      memset(com, 0, 1024);
      l = 0;
      sprintf(com, command[l]);
      l++;
      while (com[l ]) {
        strcat(com, temp_ws);
        strcat(com, command[l]);
        l++;
      }
      strcpy(Job[loop].command_ip, com);
      Job[loop].pid = job_info_param->pid;
      break;
    }
  }
}

void PrintBackgroundJob(JobInfo *Job) {
  int loop = 0;
  int loop2 = 0;
  char message[512];
  int stat;
  pid_t pid;
  for ( ; loop < MAX_BACKGROUND_JOBS; loop++) {
    if (0 != Job[loop].jid) {
      pid = waitpid(Job[loop].pid , &stat, WNOHANG);
      if (0 == pid) {
        loop2 = 0;
        memset(message, 0, 512);
        sprintf(message, "%d : ", Job[loop].jid);
        strcat(message, Job[loop].command_ip);
        strcat(message, "\n");
        write(STDOUT_FILENO, message, strlen(message));
      }
    }
  }
}

void BackgroundJobHandler(char ** command, JobInfoParam *job_info_param) {
  int *is_bg_job = job_info_param->is_bg_job;
  int loop = 0;
  *is_bg_job = 0;
  while (command[loop]) {
    if (0 == strcmp(command[loop], "&")) {
      // if( NULL == command[loop+1]) {
        *is_bg_job = 1;
        command[loop] = NULL;
        AddBackgroundJob(command, job_info_param);
      // }
    }
    loop++;
  }
}

void UpdatePidJidMap(pid_t pid, JobInfoParam *job_info_param) {
  JobInfo *Job = job_info_param->Job;
  int *jid = job_info_param->jid;
  int loop = 0;
  for ( ; loop < MAX_BACKGROUND_JOBS; loop++) {
      if (*jid == Job[loop].jid) {
     // Add to waiting job
        Job[loop].pid = pid;
        break;
      }
  }
}

int myAtoi(char *str, int * res) {
  *res = 0;
    int loop = 0;
    while (str[loop]) {
      if (str[loop] >= '0' && str[loop] <= '9')
        *res = *res * 10 + str[loop] - '0';
      else
        return 0;
      loop++;
    }
    return 1;
}

void WaitforJob(int jid, JobInfo *Job) {
  int loop = 0;
  int child_stat;
  time_t t1, t2;
  if (DEBUG)
  printf("__FUNCTION__ = %s\n", __FUNCTION__);
  char CommandError[256] = {0, };
  for (; loop < MAX_BACKGROUND_JOBS; loop++) {
    if ( jid > 0 && jid == Job[loop].jid ) {
      (void)time(&t1);
      waitpid(Job[loop].pid, &child_stat, 0);
      (void)time(&t2);
      Job[loop].jid = 0;
      Job[loop].is_free_entry = 0;
      sprintf(CommandError, JID_WAIT_TIME, (int)(t2-t1)*1000000, jid);
      write(STDOUT_FILENO, CommandError, strlen(CommandError));
      break;
    }
  }
  if (loop == MAX_BACKGROUND_JOBS) {
    sprintf(CommandError, INVALID_JID, jid);
    write(STDERR_FILENO, CommandError, strlen(CommandError));
  }
}

int  IsBuiltinCommand(char ** command) {
  int loop = 0;
  int is_builtin = 0;
  if ((NULL == command[1]) ||
      (0 == strcmp(command[1], "&") && (NULL == command[2]))) {
    if (0 == strcmp(command[0], "exit") || 0 == strcmp(command[0], "j"))
      is_builtin = 1;
  }
  if ((NULL == command[2]) ||
     ((0 == strcmp(command[2], "&") && (NULL == command[3])))) {
     if (0 == strcmp(command[0], "myw"))
     is_builtin = 1;
  }
  return is_builtin;
}

int ExecCommand(char ** command, char * command_ip) {
  static JobInfo Job[MAX_BACKGROUND_JOBS];
  static int jid = 0;
  JobInfoParam job_info_param;
  char CommandError[256] = {0, };
  int child_stat;
  int is_bg_job = 0;
  pid_t child_pid;
  int is_builtin;
  int rc;
  if (DEBUG)
  printf("__FUNCTION__ = %s1\n", __FUNCTION__);
  if (0 == strcmp(command[0], "&"))
    return 1;
  if (IsBuiltinCommand(command)) {
    if (0 == strcmp(command[0], "exit"))
      return 0;
    if (0 == strcmp(command[0], "j")) {
      PrintBackgroundJob(Job);
          return 1;
     }
     if (0 == strcmp(command[0], "myw")) {
          WaitforJob(atoi(command[1]), Job);
          return 1;
     }
  } else {
    jid++;
  }
  job_info_param.Job = Job;
  job_info_param.jid = &jid;
  job_info_param.is_bg_job = &is_bg_job;
  job_info_param.command_ip = command_ip;
  BackgroundJobHandler(command, &job_info_param);
  child_pid = fork();
    if (child_pid > 0) {
     job_info_param.pid = child_pid;
     }
  if ( 0 == child_pid ) {
    int rc = execvp(command[0], command);
    sprintf(CommandError, COMMAND_ERROR, command[0]);
    write(STDERR_FILENO, CommandError, strlen(CommandError));
    return rc;
  } else {
    if (!is_bg_job)
      waitpid(child_pid, &child_stat, 0);
  }
  return 1;
}

int ExecBatch(FILE *fp, char * buffer, char ** command ) {
  char *pos;
  char message[BUFFER_SIZE*2];
  int rc;
  int buff_len;
  close(STDIN_FILENO);
  while (NULL != fgets(buffer, BUFFER_SIZE+BUFFER_SIZE_MAX, fp)) {
    if (strnlen(buffer, BUFFER_SIZE) > BUFFER_SIZE_MAX) {
      write(STDOUT_FILENO, buffer, strlen(buffer));
       continue;
    }
    write(STDOUT_FILENO, buffer, strlen(buffer));
    ParseShellCommand(TrimWhitespace(buffer), command);
    if (feof(fp))
      fclose(fp);
    rc = 0;
    if (command[0]) {
      rc =  ExecCommand(command, buffer);
      if (rc == 0)
        break;
      }
    }
  return 0;
}

int main(int argc, char *argv[]) {
  char buffer[BUFFER_SIZE*2];
  char command_ip[BUFFER_SIZE*2];
  char ErrorMsg[1024] = {0, };
  char *command[1024];
  char *pos;
  int rc;

  while (1) {
    if (2 == argc) {
      FILE * fp = fopen(argv[1], "r");
        if (NULL == fp) {
            sprintf(ErrorMsg, BATCH_FILE_OPEN_FAIL, argv[1]);
            write(STDERR_FILENO, ErrorMsg, strlen(ErrorMsg));
           return 1;
          } else {
           rc = ExecBatch(fp, buffer, command);
           fclose(fp);
           return rc;
      }
    } else if (argc > 2) {
       write(STDERR_FILENO, INVALID_ARGUMENTS, strlen(INVALID_ARGUMENTS));
       return 1;
     } else {
       PrintShellPrompt();
       if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
         fflush(stdin);
         continue;
       } else {
     ParseShellCommand(TrimWhitespace(buffer), command);
     if (command[0]) {
       rc = ExecCommand(command, command_ip);
       if (rc == 0)
       break;
      }
    }
  }
}
  return 0;
}
