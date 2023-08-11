#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include "psush.h"
#include <fcntl.h>


unsigned short isVerbose = 0;
static char * history_queue[MAX_HISTORY] = {0};
static unsigned history_count = 0;

int 
process_user_input_simple(void)
{
    char str[MAX_STR_LEN];
    char *ret_val;
    char *raw_cmd;
    cmd_list_t *cmd_list = NULL;
    int cmd_count = 0;
    char prompt[30];
	memset(history_queue, 0, sizeof(history_queue));

    // Set up a cool user prompt.
    sprintf(prompt, PROMPT_STR " %s β: ", getenv("LOGNAME"));
    for ( ; ; ) {
        fputs(prompt, stdout);
        memset(str, 0, MAX_STR_LEN);
        ret_val = fgets(str, MAX_STR_LEN, stdin);

        if (NULL == ret_val) {
            // end of input, a control-D was pressed.
            break;
        }

        // STOMP on the pesky trailing newline returned from fgets().
        if (str[strlen(str) - 1] == '\n') {
            str[strlen(str) - 1] = '\0';
        }
        if (strlen(str) == 0) {
            // An empty command line.
            continue;
        }

        if (strcmp(str, QUIT_CMD) == 0) {

			for (int i = 0; i < MAX_HISTORY; i++)
			{
				if (history_queue[i] != NULL)
				{
					free(history_queue[i]);
					history_queue[i] = NULL;
				}
			}
	    	exit(0);
            break;
        }

        raw_cmd = strtok(str, PIPE_DELIM);

        cmd_list = (cmd_list_t *) calloc(1, sizeof(cmd_list_t));

        cmd_count = 0;
        while (raw_cmd != NULL ) {
            cmd_t *cmd = (cmd_t *) calloc(1, sizeof(cmd_t));

            cmd->raw_cmd = strdup(raw_cmd);
            cmd->list_location = cmd_count++;

            if (cmd_list->head == NULL) {
                // An empty list.
                cmd_list->tail = cmd_list->head = cmd;
            }
            else {
                cmd_list->tail->next = cmd;
                cmd_list->tail = cmd;
            }
            cmd_list->count++;

            // Get the next raw command.
            raw_cmd = strtok(NULL, PIPE_DELIM);
        }
        // Now that I have a linked list of the pipe delimited commands,
        // go through each individual command.
        parse_commands(cmd_list);

        exec_commands(cmd_list);

        free_list(cmd_list);
        cmd_list = NULL;
    }

    return(EXIT_SUCCESS);
}


void 
simple_argv(int argc, char *argv[] )
{
    int opt;

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'h':
            // help
            fprintf(stdout, "You must be out of your Vulcan mind if you think\n"
                    "I'm going to put helpful things in here.\n\n");
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            // verbose option to anything
            // I have this such that I can have -v on the command line multiple
            // time to increase the verbosity of the output.
            isVerbose++;
            if (isVerbose) {
                fprintf(stderr, "verbose: verbose option selected: %d\n"
                        , isVerbose);
            }
            break;
        case '?':
            fprintf(stderr, "*** Unknown option used, ignoring. ***\n");
            break;
        default:
            fprintf(stderr, "*** Oops, something strange happened <%c> ... ignoring ...***\n", opt);
            break;
        }
    }
}

void add_command_history(char * command)
{
	if (history_count < MAX_HISTORY)
	{
		history_queue[history_count++] = strdup(command);
	}
	
	else 
	{
		free(history_queue[0]);
		for (unsigned i = 1; i < MAX_HISTORY; i++)
		{
			history_queue[i - 1] = history_queue[i];
		}
		history_queue[MAX_HISTORY - 1] = strdup(command);
	}

}

char ** create_ragged(cmd_t * cmd)
{
	char ** ragged_array = NULL;
	param_t *curr = NULL;
	ragged_array = calloc(cmd->param_count + 2, sizeof(char*));
	ragged_array[0] = strdup(cmd->cmd);
	curr = cmd->param_list;
	for (int i = 1; i <= cmd->param_count; i++)
	{
		ragged_array[i] = curr->param;
		curr = curr->next;	
	}
	
	return ragged_array;
}

void 
exec_commands( cmd_list_t *cmds ) 
{
    cmd_t *cmd = cmds->head;
    int status;
	char *location;
	char **ragged_array = NULL;
	int fd0 = 0;
	int fd1 = 0;
	param_t *curr = NULL;
	cmd_t *curr_cmd = NULL;
	int fd_in = 0;
	int pipes[2] = {-1, -1};
	int is_first_command = 0;
	int is_last_command = 0;
	pid_t pid1 = 0;
	// pid_t pid[20];
	pid_t *pid = NULL;
	int num_loops = 0;

    if (1 == cmds->count) {
        if (!cmd->cmd) {
            // if it is an empty command, bail.
            return;
        }
		
		if (0 == strcmp(cmd->cmd, "history"))
		{
			for (unsigned int i = 0; i < history_count; i++)
			{
				printf("%u  %s\n", 1 + i, history_queue[i]);
			}	
		}

		else if (0 == strcmp(cmd->cmd, CD_CMD)) {
            if (0 == cmd->param_count) {
                // Just a "cd" on the command line without a target directory
                // need to cd to the HOME directory.
				
				location = getenv("HOME");
				if (location == NULL)
				{
					perror("Error getting home directory");
				}
				else {
					chdir(location);
				}
            }
            else {

                if (0 == chdir(cmd->param_list->param)) {
                    // a happy chdir!  ;-)
                }
                else {
                    // a sad chdir.  :-(
					perror("Could not cd into directory!");
                }
            }
        }
        else if (0 == strcmp(cmd->cmd, CWD_CMD)) {
            char str[MAXPATHLEN];

            getcwd(str, MAXPATHLEN); 
            printf(" " CWD_CMD ": %s\n", str);
        }
        else if (0 == strcmp(cmd->cmd, ECHO_CMD)) {
            // Is that an echo?
	    	curr = cmd->param_list;
	    	while (curr != NULL) {
            	printf("%s", curr->param);
				printf(" ");
				curr = curr->next;
	    	}
	    	printf("\n"); 
	
		}
        else {
            // A single command to create and exec
			pid1 = fork();

			if (pid1 == 0)
			{

				ragged_array = create_ragged(cmd);
				if(cmd->input_file_name != NULL)
				{
					fd0 = open(cmd->input_file_name, O_RDONLY);
					if (fd0 < 0)
					{
						fprintf(stderr, "redir input failed \n");
						exit(7);
					}
					dup2(fd0, STDIN_FILENO);
					close(fd0);					
				}

				if(cmd->output_file_name != NULL)
				{
					fd1 = creat(cmd->output_file_name, 00700);
					if (fd1 < 0)
					{
						perror("Could not open output file");
						exit(0);
					}
					dup2(fd1, STDOUT_FILENO);
					close(fd1);
				}

				execvp(cmd->cmd, ragged_array);
		    	perror("Child cannot exec program");
				
				_exit(EXIT_FAILURE);
	    	}

	    	else if (pid1 < 0) 
	    	{
	    		perror("error forking");
	    	}

	    	else
	    	{
				do {	
	    	  		waitpid(pid1, &status, WUNTRACED);
				} while (!WIFEXITED(status) && !WIFSIGNALED(status));
			}
    	}
	}
	else {

		pid = malloc(cmds->count * (sizeof(pid_t)));
  		curr_cmd = cmds->head;

		pipe(pipes);

		while (curr_cmd != NULL) {
			if (curr_cmd == cmds->head) {
				is_first_command = 1;
			}
			else
			{
				is_first_command = 0;
			}

			if (curr_cmd->next == NULL)
			{
				is_last_command = 1;
			}
			else
			{
				is_last_command = 0;
			}

			pipe(pipes);
			pid[num_loops] = fork();

			switch (pid[num_loops]) {
				case -1:
				{
					perror("fork failed");
					break;
				}
				
				case 0:
                {
                    // check for input redirection
                    if(is_first_command == 1 && curr_cmd->input_file_name != NULL)
                    {
                        fd0 = open(curr_cmd->input_file_name, O_RDONLY);
                        if (fd0 < 0)
                        {
                            fprintf(stderr, "redir input failed for multicommand \n");
                            exit(7);
                        }
                        dup2(fd0, STDIN_FILENO);
                        close(fd0);
                    }
                    
                    else if (is_last_command == 1) {
                        if(curr_cmd->output_file_name != NULL)
                        {
                            fd1 = creat(curr_cmd->output_file_name, 00700);
                            if (fd1 < 0)
                            {
                                perror("Could not open output file for multicommand");
                                exit(0);
                            }
							dup2(fd1, STDOUT_FILENO);
							close(fd1);
                        }
                    }
                    
                    if (is_first_command == 0)
                    {
                        if (dup2(fd_in, STDIN_FILENO) < 0)
                        {
                            perror("child process failed dup2");
                            _exit(EXIT_FAILURE);
                        }
                    }
                    
                    
                    if (is_last_command == 0)
                    {
                        dup2(pipes[1], STDOUT_FILENO);
                        close(pipes[0]);
                        close(pipes[1]);
                    }

					ragged_array = create_ragged(curr_cmd);
					execvp(curr_cmd->cmd, ragged_array);
					perror("child cannot exec program");
					_exit(EXIT_FAILURE);
				}

				default:
				{
					if (is_first_command == 0)
					{
						close(fd_in);
					}
					if (is_last_command == 0)
					{
						close(pipes[STDOUT_FILENO]);
						fd_in = pipes[0];	
					}
				}
			}

			curr_cmd = curr_cmd->next;
			num_loops++;

		} // while loop
		for (int i = 0; i < num_loops; i++)
		{
	   	  	waitpid(pid[i], &status, WUNTRACED);
		}
		free(pid);
	}
}

void
free_list(cmd_list_t *cmd_list)
{
    cmd_t *curr = cmd_list->head;

    while (NULL != curr) {
        free_cmd(curr);
        curr = curr->next;
    }
	free(cmd_list);
}

void
print_list(cmd_list_t *cmd_list)
{
    cmd_t *cmd = cmd_list->head;

    while (NULL != cmd) {
        print_cmd(cmd);
        cmd = cmd->next;
    }
}

void
free_cmd (cmd_t *cmd)
{
	param_t* param = NULL;
	param_t* temp_param = NULL;

    param = cmd->param_list;

    while (NULL != param) {
        temp_param = param->next;
		free(param->param);
		free(param);
		param = temp_param;
    }
	cmd->param_list = NULL;
	
	if (cmd->raw_cmd)
		free(cmd->raw_cmd);
	if (cmd->cmd)
		free(cmd->cmd);
	if (cmd->input_file_name)
		free(cmd->input_file_name);
	if (cmd->output_file_name)
		free(cmd->output_file_name);
	free(cmd);	
}


void
print_cmd(cmd_t *cmd)
{
    param_t *param = NULL;
    int pcount = 1;

    fprintf(stderr,"raw text: +%s+\n", cmd->raw_cmd);
    fprintf(stderr,"\tbase command: +%s+\n", cmd->cmd);
    fprintf(stderr,"\tparam count: %d\n", cmd->param_count);
    param = cmd->param_list;

    while (NULL != param) {
        fprintf(stderr,"\t\tparam %d: %s\n", pcount, param->param);
        param = param->next;
        pcount++;
    }

    fprintf(stderr,"\tinput source: %s\n"
            , (cmd->input_src == REDIRECT_FILE ? "redirect file" :
               (cmd->input_src == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
    fprintf(stderr,"\toutput dest:  %s\n"
            , (cmd->output_dest == REDIRECT_FILE ? "redirect file" :
               (cmd->output_dest == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
    fprintf(stderr,"\tinput file name:  %s\n"
            , (NULL == cmd->input_file_name ? "<na>" : cmd->input_file_name));
    fprintf(stderr,"\toutput file name: %s\n"
            , (NULL == cmd->output_file_name ? "<na>" : cmd->output_file_name));
    fprintf(stderr,"\tlocation in list of commands: %d\n", cmd->list_location);
    fprintf(stderr,"\n");
}


#define stralloca(_R,_S) {(_R) = alloca(strlen(_S) + 1); strcpy(_R,_S);}

void
parse_commands(cmd_list_t *cmd_list)
{
    cmd_t *cmd = cmd_list->head;
    char *arg;
    char *raw;

    while (cmd) {
        stralloca(raw, cmd->raw_cmd);
		add_command_history(cmd->raw_cmd);

        arg = strtok(raw, SPACE_DELIM);
        if (NULL == arg) {
            cmd = cmd->next;
            continue;
        }
        // I put something in here to strip out the single quotes if
        // they are the first/last characters in arg.
        if (arg[0] == '\'') {
            arg++;
        }
        if (arg[strlen(arg) - 1] == '\'') {
            arg[strlen(arg) - 1] = '\0';
        }
        cmd->cmd = strdup(arg);
        // Initialize these to the default values.
        cmd->input_src = REDIRECT_NONE;
        cmd->output_dest = REDIRECT_NONE;

        while ((arg = strtok(NULL, SPACE_DELIM)) != NULL) {
            if (strcmp(arg, REDIR_IN) == 0) {


                cmd->input_file_name = strdup(strtok(NULL, SPACE_DELIM));
                cmd->input_src = REDIRECT_FILE;
            }
            else if (strcmp(arg, REDIR_OUT) == 0) {
                // redirect stdout

                cmd->output_file_name = strdup(strtok(NULL, SPACE_DELIM));
                cmd->output_dest = REDIRECT_FILE;
            }
            else {
                // add next param
                param_t *param = (param_t *) calloc(1, sizeof(param_t));
                param_t *cparam = cmd->param_list;

                cmd->param_count++;
                // Put something in here to strip out the single quotes if
                // they are the first/last characters in arg.
                if (arg[0] == '\'') {
                    arg++;
                }
                if (arg[strlen(arg) - 1] == '\'') {
                    arg[strlen(arg) - 1] = '\0';
                }
                param->param = strdup(arg);
                if (NULL == cparam) {
                    cmd->param_list = param;
                }
                else {
                    // I should put a tail pointer on this.
                    while (cparam->next != NULL) {
                        cparam = cparam->next;
                    }
                    cparam->next = param;
                }
            }
        }
        // This could overwite some bogus file redirection.
        if (cmd->list_location > 0) {
            cmd->input_src = REDIRECT_PIPE;
        }
        if (cmd->list_location < (cmd_list->count - 1)) {
            cmd->output_dest = REDIRECT_PIPE;
        }

        // No need free with alloca memory.
        cmd = cmd->next;
    }

    if (isVerbose > 0) {
        print_list(cmd_list);
    }
}
