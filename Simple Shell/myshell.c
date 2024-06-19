#include "myshell_parser.h"
#include "stddef.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char *argv[]) 
{
	//check for -n
	bool n = false;
	if (argc > 1 &&	strcmp(argv[1], "-n") == 0)
		n=true;

	//make sring for command line and pipeline struct for pipe
	char *in = malloc(MAX_LINE_LENGTH);
	struct pipeline *Pipe;

	//variables to keep track of children count/current child
	int childcount = 0;

	//variable just for storing the wait pid status
	int status;

	//infinite loop
	while(true)
	{
		//prompt, only print if not -n
		if(!n)
			printf("my_shell$ ");
		fflush(stdout);

		//read input, if ctrl d then break (null input)
		if(fgets(in, MAX_LINE_LENGTH, stdin)  == NULL)
		{
			break;
		}

		//if newline just go to next loop
		if(in[0] == '\n')
			continue;

		//parse
		Pipe = pipeline_build(in);
		//invalid input will lead to skipping this loop
		if(Pipe==NULL)
		{
			perror("ERROR");
			continue;
		}

		struct pipeline_command *currentcommand = Pipe->commands;

		//count amount of children there will be
		childcount=0;
		while(currentcommand!=NULL)
		{
			childcount++;
			currentcommand = currentcommand->next;
		}
		//make current command first command again before rest of code
		currentcommand = Pipe->commands;

		//for pipe file directories
		int *fd = malloc(sizeof(2*(childcount-1)));	//hold the fd for read and write of pipes

		//pipe
		int curr = 0;
		//amount of pipes = childcount-1, 2*that = amount of read/write fds
		for(curr = 0; curr<2*(childcount-1);curr+=2)
		{
			pipe(&fd[curr]);
		}

		//reset current child counter
		curr=-2;

		//another while loop until all commands executed
		while(currentcommand!=NULL)
		{
			//next pipe
			curr+=2;
			//children code here
			//execute all children and then wait after
			if(fork()==0)
			{
				//if input path
				if(currentcommand->redirect_in_path != NULL)
				{
					int fdin = open(currentcommand->redirect_in_path, O_RDONLY);    //read access						
					//error handling
					if(fdin<0)
						perror("ERROR");
					else
					{
						if(dup2(fdin, 0)<0)
							perror("ERROR");
						close(fdin);
					}
				}
				//if output path
				if(currentcommand->redirect_out_path != NULL)
				{
					int fdout = creat(currentcommand->redirect_out_path, 0644);     //0644 gives write, create, and wipe access
					if(fdout<0)
						perror("ERROR");
					else
					{
						if(dup2(fdout, 1)<0)
							perror("ERROR");
						close(fdout);
					}
				}

				//pipes
				if(currentcommand->next!=NULL)
				{
					//notes: curr refers to the read side, curr+1 refers to the write side; 0 refers to stdin, 1 refers to stdout
					//first child
					if(currentcommand==Pipe->commands)
					{
						//write first pipe -> stdout
						dup2(fd[curr+1],1);
						//close first pipe read end
						//close(fd[curr]);
					}
					//middle child
					else
					{
						//previous pipe read in -> stdin
						dup2(fd[curr-2],0);
						//next pipe write -> stdout
						dup2(fd[curr+1],1);
					}
				}
				else
				{
					//last child
					if(childcount>1)
					{
						//last pipe read in -> stdin
						dup2(fd[curr-2],0);
						//last pipe close write end
						//close(fd[curr-1]);
					}
				}
			
				//close pipe ptrs before exec	
				for(int i = 0; i<curr; i++)
					close(fd[i]);
				
				//execute command
				if(execvp(currentcommand->command_args[0], currentcommand->command_args)==-1)	
					perror("ERROR");
			}
			//next command
			currentcommand=currentcommand->next;
		}

		//close all pipes
		for(int i = 0; i < 2*(childcount-1);i++)
		{
			close(fd[i]);
		}

		//parent code
		for(int i = 0; i < childcount; i++)
		{
			//if foreground task then we wait 
			if(!Pipe->is_background)
				waitpid(-1, &status,0);
			//background task means dont wait
			else
				continue;
		}
		free(fd);
		pipeline_free(Pipe);
	}
	free(in);
	return 0;
}
