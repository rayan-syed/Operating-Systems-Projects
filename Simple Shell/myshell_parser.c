#include "myshell_parser.h"
#include "stddef.h"
#include <stdlib.h>
#include <string.h>

struct pipeline *pipeline_build(const char *command_line)
{
	// TODO: Implement this function
	//make pipeline
	struct pipeline *pipe = malloc(sizeof(struct pipeline));
	pipe->commands = NULL;
	pipe->is_background = 0;

	//first command struct
	struct pipeline_command *firstcommand = malloc(sizeof(struct pipeline_command));
	for(int i = 0; i < MAX_ARGV_LENGTH; i++)
	{
		firstcommand->command_args[i] = NULL;
	}
	firstcommand->redirect_in_path = NULL;
	firstcommand->redirect_out_path = NULL;
	firstcommand->next = NULL;

	//current command struct
	struct pipeline_command *currentcommand = NULL;

	//current command should be firstcommand for now
	pipe->commands = firstcommand;
	currentcommand = firstcommand;

	//index variable to traverse command arguments
	int argindex = 1; //index 0 is for command

	//index for traversing command line
	int i = 0;
	
	//traverse till end
	while(command_line[i]!='\0')
	{
		//skip spaces
		if(command_line[i] == ' ' || command_line[i] == '\n' || command_line[i] == '\t')
			//just go to next char
			i++;
	
		//background
		else if(command_line[i] == '&')
		{
			//& means background
			pipe->is_background = 1;
			
			//next char
			i++;
		}

		//out
		else if(command_line[i] == '>')
		{
			//move one char forward befoer doing anything
			i++;

			//if there's already redirect out path then invalid
			if(currentcommand->redirect_out_path != NULL)
			{
				pipeline_free(pipe);
				return NULL;
			}
			//skip spaces until text to save as file
			while(command_line[i]==' ' || command_line[i] == '\n' || command_line[i] == '\t')
				i++;
			//make sure that the first character is valid for redirect out path - otherwise return null
			if(command_line[i] == '&' || command_line[i] == '>' || command_line[i] == '<' || command_line[i] == '|')
			{
				pipeline_free(pipe);
				return NULL;
			}
			//save all text as file (all text until space or token), make string to hold the output
			char *out = malloc(MAX_LINE_LENGTH);
			//index for traversing out ^
			int j = 0;
			while(command_line[i] != ' ' && command_line[i] != '\t' && command_line[i] != '\n' && command_line[i] != '&' &&	command_line[i] && '>' != command_line[i] && '<' && command_line[i] != '|')
			{
				//save characters into out one at a time until text file done
				out[j] = command_line[i];
				j++;
				i++;
			}
			currentcommand->redirect_out_path = out;

			//no need to i++ because it was done in while loop above
		}

		//in
		else if(command_line[i] == '<')
		{
			//move one char forward before doing anything
			i++;

			//if there's already redirect in path then invalid
			if(currentcommand->redirect_in_path != NULL)
			{
				pipeline_free(pipe);
				return NULL;
			}
			//skip spaces until text to save as file
			while(command_line[i]==' ' || command_line[i] == '\n' || command_line[i] == '\t')
				i++;
			//make sure that the first character is valid for redirect in path - otherwise return null
			if(command_line[i] == '&' || command_line[i] == '>' || command_line[i] == '<' || command_line[i] == '|')
			{
				pipeline_free(pipe);
				return NULL;
			}
			//save all text as file (all text until space or token), make string to hold the output
			char *in = malloc(MAX_LINE_LENGTH);
			//index for traversing in ^
			int j = 0;
			while(command_line[i] != ' ' && command_line[i] != '\t' && command_line[i] != '\n' && command_line[i] != '&' && command_line[i] != '>' && command_line[i] != '<' && command_line[i] != '|')
			{
				//save characters into out one at a time until text file done
				in[j] = command_line[i];
				j++;
				i++;
			}
			currentcommand->redirect_in_path = in;
			
			//no need to i++ because it was done in while loop above
		}
	
		//pipe
		else if(command_line[i] == '|')
		{
			//reset argindex to 1
			argindex = 1;
			//need new command, initialize everything to NULL
			struct pipeline_command *nextcommand = malloc(sizeof(struct pipeline_command));
			for(int i = 0; i < MAX_ARGV_LENGTH; i++)
			{
				nextcommand->command_args[i] = NULL;
			}
			nextcommand->redirect_in_path = NULL;
			nextcommand->redirect_out_path = NULL;
			nextcommand->next = NULL;

			//next command is nextcommand
			currentcommand->next = nextcommand;
			//next command is now current
			currentcommand = nextcommand;

			//next char
			i++;
		}
	
		//arg
		else
		{
			//need to save argument
			char *arg = malloc(MAX_ARGV_LENGTH);
			//traverse until invalid char
			int j = 0;	//index for traversing
			//traverse
			while(command_line[i] != ' ' && command_line[i] != '\n' && command_line[i] != '\t' && command_line[i] != '&' && command_line[i] != '>' && command_line[i] != '<' && command_line[i] != '|')
			{
				arg[j] = command_line[i];
				j++;
				i++;
			}


			//check if command saved, if not save command, else its argument
			if(currentcommand->command_args[0] == NULL)
				//token is command (index 0)
				currentcommand->command_args[0] = arg;
			else
			{
				//token is argument, index argindex after
				currentcommand->command_args[argindex] = arg;
				argindex++;
			}

			//no need to i++ since done in while loop above
		}

		//go to next char in the command line string
		//i++ is performed at end of each if/else loop
	}

	//other things that need to be freed get freed in pipeline_free so we can return pipe now
	return pipe;
}

void pipeline_free(struct pipeline *pipeline)
{
	// TODO: Implement this function
	//keep track of current command
	struct pipeline_command *currentcommand = pipeline->commands;

	//traverse until end of commands
	while(currentcommand!=NULL)
	{
		//free all the members of command
		for(int i = 0; i<MAX_ARGV_LENGTH; i++)
		{
			free(currentcommand->command_args[i]);
		}
		free(currentcommand->redirect_in_path);
		free(currentcommand->redirect_out_path);
		
		//save next command then erase current command
		struct pipeline_command *nextcommand = currentcommand->next;
		free(currentcommand);

		//current command is now next one, loop
		currentcommand = nextcommand;
	}

	//once all commands freed up then free the pipe itslef
	free(pipeline);
}
