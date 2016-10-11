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
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE          512
#define ERROR_MSG_SIZE       256
#define MAX_BACKGROUND_JOBS  64

#define SHELL_PROMPT         "mysh> "
#define INVALID_ARGUMENTS    "Usage: mysh [batchFile]\n"
#define BATCH_FILE_OPEN_FAIL "Error: Cannot open file %s\n"
#define COMMAND_ERROR        "%s: Command not found\n"
#define INVALID_JID          "Invalid jid %d\n"
#define JID_WAIT_TIME        "%d : Job %d terminated\n"
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
  int loop2 = 0;
  for( ; loop < MAX_BACKGROUND_JOBS; loop++) {
    
    if( 0 == Job[loop].is_free_entry) { // 0 => free, 1 => occupied
      Job[loop].is_free_entry = 1;
      Job[loop].jid = *jid;
      while(command[loop2]) {
        Job[loop].command[loop2] = strdup(command[loop2]);
        loop2++;
      }
      Job[loop].command[loop2] = NULL;
      break;
    }
  }
}

void PrintBackgroundJob( JobInfo *Job) {
  int loop = 0;
  int loop2 = 0;
  char message[512];
  int stat;
  for( ; loop < MAX_BACKGROUND_JOBS; loop++) {
  
    if(1 == Job[loop].is_free_entry) {
      if(0 !=  waitpid(Job[loop].pid, &stat, WNOHANG))
        continue;

      loop2 = 0;
      sprintf(message, "%d :", Job[loop].jid);
      while(Job[loop].command[loop2]) {
        strcat(message, " ");
        strcat(message, Job[loop].command[loop2]);
        loop2++;
      }
      strcat(message, "\n");
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

void UpdatePidJidMap(pid_t pid, JobInfoParam *job_info_param) {
  
  static JobInfo *Job_s;
  JobInfo *Job = job_info_param->Job;
  int *jid = job_info_param->jid;
  int loop = 0;
  
  if( Job)
    Job_s = Job;

  for( ; loop < MAX_BACKGROUND_JOBS; loop++) {
    
    if( NULL == jid) {   // Remove from waiting job
      if( pid == Job_s[loop].pid) {
	//Job_s[loop].jid = 0;
        Job_s[loop].is_free_entry = 0;
	break;
      }

    } else if( *jid == Job[loop].jid) { // Add to waiting job
      Job[loop].pid = pid;
      break;
    }
  }
}

void WaitforJob(int jid, JobInfo *Job) {
  int loop = 0;
  int child_stat;
  time_t t1,t2;
 
  char CommandError[256] = {0, };
  for( ; loop < MAX_BACKGROUND_JOBS; loop++) {
    if( jid == Job[loop].jid) {
      (void) time(&t1);
      waitpid(Job[loop].pid, &child_stat, 0);
      (void) time(&t2);
      Job[loop].jid = 0;
      Job[loop].is_free_entry = 0;
      sprintf(CommandError, JID_WAIT_TIME,(int)(t2-t1)*1000000, jid);
      write(STDOUT_FILENO, CommandError, strlen(CommandError));
      break;
    } 
  }
  if( loop == MAX_BACKGROUND_JOBS) {
    sprintf(CommandError, INVALID_JID, jid);
    write(STDOUT_FILENO, CommandError, strlen(CommandError));
  }
}

void child_termination_handler( int signum) {
  pid_t pid;
  int status;
  JobInfoParam job_info_param;
  memset (&job_info_param, 0, sizeof (job_info_param));
  if(SIGCHLD == signum) {
    pid = wait(&status);
    if(-1 == pid) {
    
    } else {
      // UpdatePidJidMap(pid, &job_info_param);
    }
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
  struct sigaction act;
  memset (&act, 0, sizeof (act));
  act.sa_handler = child_termination_handler;
  //  sigaction(SIGCHLD, &act, NULL); 
  if(0 == strcmp(command[0],"exit"))
    return 0;
  if(0 == strcmp(command[0],"j")) {
    PrintBackgroundJob(Job);
    return 1;
  }
  if(0 == strcmp(command[0],"myw")) {
    WaitforJob(atoi(command[1]), Job);
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
    else
    UpdatePidJidMap(child_pid, &job_info_param);
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
