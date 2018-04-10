#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include "builtins.h"

int ownexit(char *argv[]);
int ownecho(char *argv[]);
int owncd(char *argv[]);
int ownkill(char *argv[]);
int ownls(char *argv[]);

builtin_pair builtins_table[]={
	{"exit",	&ownexit},
	{"lecho",	&ownecho},
	{"lcd",		&owncd},
	{"lkill",	&ownkill},
	{"lls",		&ownls},
	{NULL,NULL}
};


int ownexit(char * argv[])
{
	_exit(0);
    return BUILTIN_ERROR;
}

int ownecho( char * argv[])
{
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int owncd(char * argv[])
{
    if (argv[2] != NULL)
    {
        return BUILTIN_ERROR;
    }
    
	char* path;
	if (argv[1] == NULL)
	{
		path = getenv("HOME");
	}
	else
	{
		path = argv[1];
	}
	return chdir(path);
}

int ownkill(char * argv[])
{
	if (argv[1] == NULL)
	{
		return BUILTIN_ERROR;
	}
    int sig, pid;
    if (argv[1] != NULL && argv[2]!= NULL)
    {
        sig = strtol(argv[1]+1, NULL, 10);
        pid = strtol(argv[2], NULL, 10);
    }
    else
    {
        sig = SIGTERM;
        pid = strtol(argv[1], NULL, 10);
    }
	return kill(pid,sig);
}

int ownls(char * argv[])
{
	DIR *dp;
  	struct dirent *ep;     
  	dp =  opendir(".");;

  	if (dp != NULL)
  	{
    	    while ((ep = readdir(dp)))
            {
                if (ep->d_name[0] != '.')
              		printf("%s\n", ep->d_name);
            }

    		return closedir (dp);
  	}
	fflush(stdout);
  	return BUILTIN_ERROR;	
}
