
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

struct stat buf_stat;

char buff[2*MAX_LINE_LENGTH+5];
int read_len;
int buff_end;

char line_to_pars[MAX_LINE_LENGTH+1];
int line_overflow;


volatile int amountofforegoundproc = 0;
volatile int foregroundsize = 0;
volatile int foregroundprc[MAX_SIZE_ENDED_PROC]; //?
volatile int endedsize = 0;
volatile int endedproc[MAX_SIZE_ENDED_PROC][2];

void buffer_init()
{
    buff_end=0;
    line_overflow=0;
}

void shift_buff(int pos)
{
    pos++;
    buff_end = buff_end - pos;
    for (int i = 0; i < buff_end; ++i)
    {
        buff[i] = buff[pos+i];
    }
}

int read_line()
{
    if (line_overflow == 1)
    {
        for (int i = 0; i < buff_end; i++)
        {
            if(buff[i]=='\n')
            {
                shift_buff(i);
                line_overflow=0;
                //uff_end -= i;
                return 2;
            }
        }
        buff_end = 0;
        //return 2;
    }
    for (int i = 0; i < buff_end;i++)
    {
        if(buff[i]=='\n')
        {
            line_to_pars[i]=0;
            shift_buff(i);
            return 0;
        }
        if(i==MAX_LINE_LENGTH)
        {
            line_overflow = 1;
            shift_buff(i);
            return 3;
        }
        line_to_pars[i]=buff[i];
    }
	read_len = read(STDIN_FILENO, buff + buff_end, MAX_LINE_LENGTH);
    //printf("%d\n", read_len/8);
    if (read_len == -1)
    {
        if(errno == EINTR) return 2; //SIGCHLD while read()
        return -1;
    }
	if (read_len == 0)
	{
		buff[buff_end++]=0;
		for(int i=0;i<buff_end;i++)
        {
            line_to_pars[i]=buff[i];
        }
        buff_end=0;
        return 1; 
	}
    buff_end += read_len;
    return 2;
}

void handle_redirerr(char* filename)
{
    if(errno == EACCES)
    {
        fprintf(stderr, "%s: permission denied\n", filename);
    }
    else if(errno == ENOENT)
    {
        fprintf(stderr, "%s: no such file or directory\n", filename);
    }
    exit(-1);
}

void redirect(redirection *status)
{
    int fd;
    if (status == NULL)
    {
        return;
    }
    if (IS_RIN(status->flags))
    {
        close(0);
        fd = open(status->filename, O_RDONLY);
        if (fd != 0)
        {
            handle_redirerr(status->filename);
        }
    }
    else if(IS_RAPPEND(status->flags))
    {
        close(1);
        fd = open(status->filename,O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH);
        if (fd != 1)
        {
            handle_redirerr(status->filename);
        }
    }
    else if (IS_ROUT(status->flags))
    {
        close(1);
        fd = open(status->filename,O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH);
        if (fd != 1)
        {
            handle_redirerr(status->filename);
        }
    }
}

int getLengthOfPipeline(pipeline p)
{
    int length=0;
    while(p[length] != NULL)
    {
        length++;   
    }
    return length;
}

int hasPipelineEmptyCommand(pipeline p)
{
    int length = getLengthOfPipeline(p);
    for (int i = 0; i < length; ++i)
    {  
        if (p[i]->argv[0] == NULL && length >1)
        {
            fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            return -1;
        }
    }
    return 1;
}

int hasLineEmptyCommand(line* ln, int amountOfPipelines)
{
    for(int i=0; i< amountOfPipelines;i++)
    {
        if(hasPipelineEmptyCommand(ln->pipelines[i]) == -1)
        {
            return -1;
        }
    }
    return 0;
}

int checkIsfShellCom(char comName[])
{
    
	int i = 0; 
	for (int i = 0; i < 5; ++i)
	{
		if(!strcmp(comName, builtins_table[i].name))
		{
			return i;
		}
	}
	return -1;
}

void executeShellCom(int num, char* argv[])
{
    if (builtins_table[num].fun(argv) == -1)
    {
        fprintf(stderr, "Builtin %s error.\n", builtins_table[num].name);
    }
}

void waitForForegroundProcesses()
{
    sigset_t emptymask;
    sigemptyset(&emptymask);

    while( amountofforegoundproc>0 )
    {
        sigsuspend(&emptymask);
    }
    foregroundsize = 0;
}

int parse_input(char* input)
{
    line* ln = parseline(input);

    if( ln == NULL )
    {
        if( write(2,SYNTAX_ERROR_STR,strlen(SYNTAX_ERROR_STR)) == -1 ) 
        {
            exit(-1);
        }
    }

    int amountOfPipelines = 0;

    while (ln->pipelines[amountOfPipelines]!= NULL)
    {
        amountOfPipelines++;
    }

    if(hasLineEmptyCommand(ln, amountOfPipelines) == -1)
    {
        return -1;
    }
    
    command* com;

    // W przypadku gdy pipeline zawiera więcej niż jedno polecenie można założyć że żadne z nich nie
    // jest komendą wbudowaną shella.
    for(int i=0; i< amountOfPipelines;i++)
    {
        pipeline p = ln->pipelines[i];
        int pipelineLength = getLengthOfPipeline(p);
        if (pipelineLength == 1)
        {  
            com = p[0];
            if (com->argv[0] == NULL)
            {  
                continue;
            }
            int num = checkIsfShellCom(com->argv[0]);
            if (num >= 0)
            {
                //dlaczego nie w potomnym
                executeShellCom(num, com->argv);
                continue;
            }
        }
    
        sigset_t newMask;
        sigemptyset(&newMask);
        sigaddset(&newMask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &newMask, NULL);
        
        int fdOld[2];
        int fdNew[2];
        int pid;
        fdNew[0] = -42;
        fdNew[0] = -42;  
        
        for(int comNumber = 0; comNumber < pipelineLength; ++comNumber)
        {
            com = p[comNumber];
            if( com == NULL )
            {
                if( write(2,SYNTAX_ERROR_STR,strlen(SYNTAX_ERROR_STR))==-1 )
                {
                    exit(-1);
                }
            }

            fdOld[0] = fdNew[0];
            fdOld[1] = fdNew[1];
            if (pipe(fdNew) == -1)
                return -1;
            
		    pid = fork();
		    if (pid == -1)
		    {
			    exit(-1);
		    }
		
		    if (pid > 0) //parent
		    {

                if(ln->flags != LINBACKGROUND)
                {
                    amountofforegoundproc++;
                    foregroundprc[foregroundsize++] = pid;
                }
			    if(comNumber > 0)
                {
                    if (close(fdOld[0]) == -1)
                        exit(1);
                    if (close(fdOld[1]) == -1)
                        exit(1);
                }
                if(comNumber == pipelineLength-1)
                {
                    if (close(fdNew[0]) == -1)
                        exit(1);
                    if (close(fdNew[1]) == -1)
                        exit(1);
                    waitForForegroundProcesses();
                }
		    }
		    else //child
		    {
                sigprocmask(SIG_BLOCK, &newMask, NULL);

                struct sigaction sigaint;
                sigaint.sa_handler = SIG_DFL;
                sigaint.sa_flags = 0;
                sigemptyset(&sigaint.sa_mask);
                sigaction(SIGINT, &sigaint, NULL);
                
                if(ln->flags == LINBACKGROUND ) 
                {
                    if( setsid() == -1) 
                        exit(1);
                }

                if(comNumber > 0)
                {
                    if (close(fdOld[1]) == -1)
                        exit(1);
                    if (dup2(fdOld[0],0) == -1)
                        exit(1);
                    if (close(fdOld[0]) == -1)
                        exit(1);
                }
                if(comNumber != pipelineLength-1)
                {
                    if (dup2(fdNew[1],1) == -1)
                    {
                        exit(1);
                    }
                }
                if (close(fdNew[0]) == -1)
                {
                    exit(1);
                }
                if (close(fdNew[1]) == -1)
                {
                    exit(1);
                }
 
                for (int i=0; com->redirs[i] != NULL; ++i)
                {
                    redirect(com->redirs[i]);
                }
                
                if( com->argv==NULL || com->argv[0]==NULL)
                {
                    exit(1);
                }

			    if (execvp(com->argv[0], com->argv) == -1)
                {
		            if (errno == ENOENT)
		            {
		                fprintf(stderr, "%s: no such file or directory\n", com->argv[0]);
		            }
		            else if (errno == EACCES)
		            {
		                fprintf(stderr,"%s: permission denied\n", com->argv[0]);
		            }
		            else
		            {
		                fprintf(stderr, "%s: exec error\n", com->argv[0]);
		            }
		            exit(EXEC_FAILURE);
                }
		    }
        }
        sigprocmask(SIG_UNBLOCK, &newMask, NULL);
    }
    return 0;
}

int read_input()
{
    int status;

    while(1)
    {
        status = read_line();
        if(status == 0 || status ==1)
        {
            parse_input(line_to_pars);
        }
        if (status == 1 || status ==-1) //EOF or fatal error
        {
            return status;
        }
        else if (status == 3)
		{
			fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
	    }
        if(buff_end == 0 && status == 0)
        {
            return 0;
        }
    }
}

void print_status_background_processes()
{
    sigset_t newmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &newmask, NULL);

    for(int i=0; i<endedsize; ++i)
    {
        if( WIFEXITED(endedproc[i][1]) )
        {
            fprintf(stdout,"Background process %d terminated. (exited with status %d)\n", 
            endedproc[i][0], WEXITSTATUS(endedproc[i][1]));
        }
        else if ( WIFSIGNALED(endedproc[i][1]) )
        {
            fprintf(stdout,"Background process %d terminated. (killed by signal %d)\n",
            endedproc[i][0],WTERMSIG(endedproc[i][1]));
        }
    }
    endedsize = 0;
    sigprocmask(SIG_UNBLOCK, &newmask, NULL);
}

int fromForeground(int pid)
{
    for(int i=0;i<foregroundsize;i++)
    {
        if(foregroundprc[i] == pid) return 1;
    }
    return 0;
}

void sigafromchild_handler(int sig)
{
    int errnoS = errno;
    int pid = 0, status;
    while( (pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if( fromForeground(pid))
        {
            amountofforegoundproc--;
        }
        else
        {
            if(endedsize < MAX_SIZE_ENDED_PROC)
            {
                endedproc[endedsize][0] = pid;
                endedproc[endedsize][1] = status;
                endedsize++;
            }
        }
    }
    errno = errnoS;
}

int main(int argc, char *argv[])
{
    struct sigaction sigafromchild;
    sigafromchild.sa_handler = sigafromchild_handler;
    sigafromchild.sa_flags = 0;
    sigemptyset(&sigafromchild.sa_mask);
    sigaction(SIGCHLD, &sigafromchild, NULL);

    struct sigaction sigaint;
    sigaint.sa_handler = SIG_IGN;
    sigaint.sa_flags = 0;
    sigemptyset(&sigaint.sa_mask);
    sigaction(SIGINT, &sigaint, NULL);
	
	pid_t pid;
	int status, err;

	int print_prompt = 0;

	if (fstat(0, &buf_stat) == -1)
		exit(1);

	if (S_ISCHR(buf_stat.st_mode))
		print_prompt = 1;

    buffer_init();

	while (1)
	{
		if (print_prompt)
		{
            print_status_background_processes();
			fprintf(stdout, "$ ");
		   	fflush(stdout);
		}

        status = read_input();
        if (status == -1) //Fatal error
		{
		    exit(-1);
		}
		if (status == 1) //EOF
		{
			break;
		}
	}
	return 0;
}
