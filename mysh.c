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

#define BUFFER_SIZE          512
#define ERROR_MSG_SIZE       256
#define MAX_BACKGROUND_JOBS  64

#define SHELL_PROMPT         "mysh> "
#define INVALID_ARGUMENTS    "Usage: mysh [batchFile]\n"
#define BATCH_FILE_OPEN_FAIL "Error: Cannot open file %s\n"
#define COMMAND_ERROR        "%s: Command not found\n"

typedef struct JobStruct {
  int jid;
  int job_status;
  int is_free_entry;
  int wait_time;
  pid_t pid;
  char *command[32];
}JobInfo;

typedef struct JobInfoParamStruct {
  JobInfo *Job;
  int *jid;
  int * is_bg_job;
}JobInfoParam;

void ErrorMessage(int line,const char *file) {
  
  char number[22];
  char error_message[ERROR_MSG_SIZE] = "Error : file, ";
  strncat (error_message, file, strlen(file));
  strcat (error_message, " at line ");
  sprintf(number, "%d ", line);
  strncat(error_message, number, strlen(number));
  strcat(error_message, "\n");
  write(STDERR_FILENO, error_message, strlen(error_message));
}

void TrimWhitespace (char *str) {
  char *str_ptr=str;
  while( isspace(*str_ptr)) 
    str_ptr++;
  if(0 == (*str_ptr)) {
    str_ptr[0] = 0;
    return;
  }

  int loop=0;
  do {
    str[loop++] = *str_ptr++;
  } while(*str_ptr);
 
  str_ptr--;
  while(loop > 0 && isspace(*str_ptr) )
    loop-- , str_ptr-- ;
  
  str[loop] = 0;
  return;
}

void PrintShellPrompt( ) {
  char ShellPrompt[] = SHELL_PROMPT;
  write(STDOUT_FILENO, ShellPrompt, strlen(ShellPrompt));
}

void ParseShellCommand (char *command_buf, char ** command) {
  char delimeter[] = " ";
  int loop=0;
  char *token = strtok(command_buf, delimeter);
  while( NULL != token) {
    command[loop++] = token;
    token = strtok(NULL, delimeter);
    while( token && isspace( *token))
      token++;
  }
  command[loop] = NULL;
}

void AddBackgroundJob(char ** command, JobInfoParam *job_info_param) {
  
  JobInfo *Job = job_info_param->Job;
  int *jid = job_info_param->jid;
  int loop = 0;

  for( ; loop < MAX_BACKGROUND_JOBS; loop++) {
    
    if( 0 == Job[loop].is_free_entry) {
      Job[loop].is_free_entry = 1;
      Job[loop].jid = *jid;
      break;
    }
  }
}
void PrintBackgroundJob( JobInfo *Job) {
  int loop = 0;
  char message[32];
  for( ; loop < MAX_BACKGROUND_JOBS; loop++) {
    if(1 == Job[loop].is_free_entry) {
      sprintf(message, "%d\n", Job[loop].jid);
      write(STDOUT_FILENO, message, strlen(message));
    }
  }
}
void BackgroundJobHandler(char ** command, JobInfoParam *job_info_param) {
  
  int *is_bg_job = job_info_param->is_bg_job;
  int loop=0;
  *is_bg_job = 0;
  while( command[loop]) {
    if(0 == strcmp(command[loop],"&")) {
      if( NULL == command[loop+1]) {
        *is_bg_job = 1;
        command[loop] = NULL;
        AddBackgroundJob(command, job_info_param);
      }
    }
      loop++;
  }
}

int ExecCommand( char ** command) {
  static JobInfo Job[MAX_BACKGROUND_JOBS];
  static int jid = 0;
  JobInfoParam job_info_param;
  char CommandError[256] = {0, };
  int child_stat;
  int is_bg_job=0;
  pid_t child_pid;
  
  if(0 == strcmp(command[0],"exit"))
    return 0;
  if(0 == strcmp(command[0],"j")) {
    PrintBackgroundJob(Job);
    return 1;
  }  
  jid++;
  job_info_param.Job = Job;
  job_info_param.jid = &jid;
  job_info_param.is_bg_job = &is_bg_job;
  BackgroundJobHandler(command, &job_info_param);
  child_pid = fork();
  if(0 == child_pid) {
    int rc = execvp(command[0],command);
    sprintf(CommandError,COMMAND_ERROR,command[0]);
    write(STDERR_FILENO,CommandError,strlen(CommandError));
       return rc;
  } else {
    if( ! is_bg_job)
    waitpid(child_pid, &child_stat, 0);
  }
    return 1;
}

int ExecBatch(FILE *fp, char * buffer, char ** command ) {
  char *status = NULL;
  while((status =  fgets(buffer, BUFFER_SIZE, fp))) {
    TrimWhitespace (buffer);
    ParseShellCommand(buffer, command);
    write(STDOUT_FILENO,buffer,strlen(buffer));
    write(STDOUT_FILENO,"\n",strlen("\n"));
    int rc =  ExecCommand(command);
    if( 0 == rc)
      return 0;
  }
  if( NULL == status)
    return 0;
  return 1;
}

int main(int argc, char *argv[]) {

  char buffer[BUFFER_SIZE];
  char ErrorMsg[256] = {0, };
  char *command [32];
  while (1) { 

    if( 2 == argc && strlen (argv[1])) { 
        FILE * fp = fopen(argv[1], "r");
        if( NULL == fp) {
          sprintf(ErrorMsg, BATCH_FILE_OPEN_FAIL, argv[1]);
	  write (STDERR_FILENO, ErrorMsg, strlen(ErrorMsg));
          return 1;
	} else {
          int rc = ExecBatch(fp, buffer, command);
          if( 0 == rc)
            return 0;
	} 
    } 
   else if(argc > 2) {
         write (STDERR_FILENO, INVALID_ARGUMENTS, strlen(INVALID_ARGUMENTS));
        return 1;
   } else {
    PrintShellPrompt();
    if(fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      if( feof(stdin)) {
        return 0;
      }
      ErrorMessage(__LINE__, __FILE__);
      continue;
    }
    TrimWhitespace (buffer);
    ParseShellCommand(buffer, command);
   int rc =  ExecCommand(command);
   if( 0 == rc)
     return 0;
   }
  }
  return 0;
}
