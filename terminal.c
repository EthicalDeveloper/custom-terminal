#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include<sys/wait.h>
#include<unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

char* InputString();
char** GetArgs(char* com);
void Free(char** args);
char** GetCommands(char** line);
//void PipedCommands(char* line); //former pipe handling function
void PipesAndRedirection(char* line);
int batch = 0; //Acts as bool for if there is a batch file given at command line
FILE* bFile; //This is just to make a quick tweek to the file if necssary to prevent any undefined behavior and keep track of where we are in the file
int b_fd; //to duplicate batch file to stdin
int stdin_cpy; //to retiain a copy of stdin file after closing stdin.
void cd_run( char** args ); //Lets the program know that cd_run function exists

void print_history(); // prints the whole array
void delete_history(); // deletes all the commands
void add_history( const char *cmd ); // stores the command history in the array
static char *history[20]; //pointer array to store the commands
int count = 0; // count of the commands
char* flag_1; // character variable to store the flag
char* flag_2; // character variable to store the second flag

//int pid;
pid_t pid;
void signal_handler(int signal); // signal handler. 

//Helper funcion
void RemoveWhitspace(char** str); //pass in an char* str like this ---> RemoveWhitspace(&str);

int main(int argc, char** argv)
{
    //pid_t pid;
	int status;
	int fd;
	int exitCom = 0; //acts as bool to check if an exit command was given.
	char* line;
	
	signal(SIGINT,signal_handler);
	signal(SIGTSTP,signal_handler);
	
	if(argc > 1) //check if batch file was given.
	{
		/*
		if(freopen(argv[1], "r", stdin) == NULL) //I added this in case the file isn't found
		{
			printf("\nCould not open \"%s\". Terminated\n\n", argv[1]);
			exit(1);
		}
		*/
		//The following is to append a newline at the end of the file (if there isn't one).
		//For some reaosn, there seems to be some undefined behavior if the input file isn't
		//stricitly ended with a newline.
		bFile = fopen(argv[1], "r+"); //open for reading and updating
		if(bFile == NULL)
		{
			printf("\nCould not open \"%s\". Terminated\n\n", argv[1]);
			exit(1);
		}
		fseek(bFile, -1, SEEK_END); //go to last character of file
		if(fgetc(bFile) != '\n') //If last character is not a newline, append a newline to the file.
		{
			fprintf(bFile, "\n");
		}
		fclose(bFile); //close the file.
		
		bFile = fopen(argv[1], "r"); //open file to keep track of when it ends, so we know when to switch back to interactive mode
		stdin_cpy = dup(fileno(stdin)); //keep track of stdin file.
		b_fd = open(argv[1], O_RDONLY); //open file again (with file descriptor this time) to duplicate the batch file to stdin
		dup2(b_fd, 0); //duplicate to stdin so program takes input from batch file
		close(b_fd);
		batch = 1;
	}
	
	
	//int i=0; //this was used for debugging purposes
    while(1)
    {
		printf("\n");
		char** coms = GetCommands(&line);
		for(int i=0; coms[i] != NULL; ++i) //loop goes through each command returned from GetCommands(...)
		{
		    add_history(coms[i]);
			int pipesAndRedirection = 0; //acts as a bool. if there is a '|', '>', or '<' in the command statement, then it should be handled by the PipesAndRedirection(...) function
			for(int j=0; j<strlen(coms[i]); ++j)
			{
				if(coms[i][j] == '|' || coms[i][j] == '>' || coms[i][j] == '<')
				{
					pipesAndRedirection = 1;
					break;
				}
			}
			char** args;
			if(pipesAndRedirection == 0)
			{
				args = GetArgs(coms[i]);
			}
			//fork and wait.
			pid = fork();
			wait(&status);
			
			if(pid == 0)
			{
				freopen("/dev/null", "r", stdin); //disconnect stdin in child
				
				if(pipesAndRedirection == 1)
				{
					/*
					PipedCommands(coms[i]); //former pipe handling function
					*/
					PipesAndRedirection(coms[i]);
					exit(1);
				}
				
				//char** args = GetArgs(coms[i]);
				if(strcmp(args[0],"exit") == 0)
				{
					exit(5); //if exit command was given, exit status will be 5 (I just used 5 becuse I felt like it).
				}
				else if(strcmp(args[0],"cd") == 0)										//
				{																		//Part added to call cd_run function
					exit(4);														//
				}
				else if (strcmp(args[0],"myhistory") == 0){
					exit(6);
				}
				else if (strcmp(args[0],"myhistory -c") == 0){
					exit(7);
				}
				else
				{
					execvp(args[0],args);
					
					perror(args[0]);
				}
				exit(1); //Normal exit exits with 1 or 0.
				
			}
			//Parent continues after child exits
			else if(pid > 0)
			{
				//check exit status of child
				if(WEXITSTATUS(status) == 5)
				{
					exitCom = 1; //set bool exitCom to 1 (true), indicating that the exit command was given
				}
				else if(WEXITSTATUS(status) == 4)
				{
					cd_run(args);
					pid_t pid_pwd = fork();
					wait(NULL);
					if(pid_pwd == 0)
					{
						execlp("pwd", "pwd", NULL);
					}
				}
				else if(WEXITSTATUS(status) == 6)
				{
					print_history();
				}
				
				else if(WEXITSTATUS(status) == 7)
				{
					delete_history();
				}
			}
		}
		if(pid > 0)
		{
			free(line);
			free(coms);
			//Now that all commands in the line were executed, check exitCom and if it is 1 (exit command was given), the shell can now exit.
			if(exitCom == 1)
			{
				printf("\n");
				exit(0);
			}
		}
/*
		if(i >= 5)
		{
			printf("\nFORCED EXIT\n");  //this was used for debugging purposes
			exit(1);
		}
		++i;
*/
    }

    return 0;
}

char* InputString()
{
    int len = 20;
    char* str = (char*)malloc(sizeof(char)*len);
	char* buff;
	unsigned int i=0;

    if(str != NULL)
    {
        int c = EOF;
		//printf("%c", fgetc(bFile));
        while( ((c = getchar()) != '\n') && (c != EOF) )
        {	
/*
			//printf("%c", fgetc(bFile));
			//fgetc(bFile);
			//if(feof(bFile))
			{
			//	printf("\n\nEnd of the line\n\n");
			//}
			*/
			
            str[i++] = (char)c;
            if(i == len)
            {
                len = len*2;
                str = (char*)realloc(str,sizeof(char)*len);
            }
        }
        str[i] = '\0';
		buff = (char*)malloc(i*2);
    }

	
	if(batch == 1)
	{
		int l = i*2; //I multiplied by 2 to ensure there was nore than enough space. Fro some reason, using i as is was cutting off the last character and doing something like i+1 or i+2 would cause an infinte loop or some undefined behavior
		if(fgets(buff, l, bFile) == NULL) //Once the end of file has been reached
		{
			buff = (char*)realloc(buff,i);
			dup2(stdin_cpy, 0); //revert input back to original stdin file so user can now enter commands interactively (this happens if exit command was not given)
			close(stdin_cpy); //close stdin_copy
			fclose(bFile); //close bFile as we have reached the end of it
			batch = 0; //make bool false and we are no more using the batch file
		}
		//printf("i: %d\n", i);
		//printf("str: %s\n", str);
		//printf("buff: %s\n", buff);
	}
	printf("\n");
    return str;
}

//User enters a line of commands (1 or more). Commands are separated with a ';' being the delimeter.
char** GetCommands(char** line)
{
    char** coms = (char**)malloc(sizeof(char*)); 
	char delim[] = ";";
	
	if(batch == 0)
	{
		printf("prompt> ");
		fflush(stdout);
	}
	
    *line = InputString();
	if(batch == 1)
		printf("%s\n\n", *line);
	
	strcat(*line, ";");
	
	int i=0;
   coms[i] = strtok(*line, delim);
   while(coms[i] != NULL)
   {
	   ++i;
	   coms = (char**)realloc(coms, sizeof(char*) * (i+1));
		coms[i] = strtok(NULL, delim);
		//printf("\ni: %d\n", i); 
   }
   
   return coms;
}
	

//A command obtained from GetCommands(...) is separated into various arguements with a space, ' ', being the delimeter.
char** GetArgs(char* com)
{
	
	char** args = (char**)malloc(sizeof(char*));
	char delim[] = " ";

    //printf("\nline: %s\n", line);
   int i=0;
   args[i] = strtok(com, delim);
   while(args[i] != NULL)
   {
	   ++i;
	   args = (char**)realloc(args, sizeof(char*) * (i+1));
		args[i] = strtok(NULL, delim);
   }
   
   return args;
}

void PipesAndRedirection(char* line)
{
	char** coms = (char**)malloc(sizeof(char*)); //to store each command in the statemnent
	char* ops = (char*)malloc(1); //to store each operator ('|', '>', '<')
	int numComs; //number of commands
	char delim[] = "|<>"; //the operators are the delimeters by which to separate the statement
	int numOps = 0; ////number of operators
	
	//This loop iterates through line and store's each opertor encountered in ops and updates numOps
	for(int i=0; i<strlen(line); ++i)
   {
	   if(line[i] == '|' || line[i] == '>' || line[i] == '<')
	   {
		   ops[numOps] = line[i];
		   ++numOps;
		   ops = (char*)realloc(ops, numOps+1);
	   }
   }
	
	//The following breaks up the line into the different commands and stores them in coms
	int i=0;
	coms[i] = strtok(line, delim);
   while(coms[i] != NULL)
   {
		++i;
		coms = (char**)realloc(coms, sizeof(char*) * (i+1));
		coms[i] = strtok(NULL, delim);
   }
   numComs = i;
   /*
   printf("numComs: %d\n", numComs);
   printf("Commands: ");
   for(int i=0; i<numComs; ++i)
   {
	   printf("%s, ", coms[i]);
   }
    printf("\nnumOps: %d\n", numOps);
   printf("Operators: ");
     for(int i=0; i<numOps; ++i)
   {
	   printf("%c, ", ops[i]);
   }
   */
   
   /*
	prompt> grep m < file1 | grep H | wc -l > file2
	
	numComs: 5
	Commands: grep m ,  file1 ,  grep H ,  wc -l ,  file2,
	numOps: 4
	Operators: <, |, |, >,
*/
   
   int fd[2]; //file descriptor array for pipe
   pid_t pid;
   int status; //keep track of status in case of error
   int prev_p = 0; //refers to the read end of previous pipe so the current pipe can read from it
   int j=0; //to iterate through the operators
   int in_out_red = 0; //bool for input/output redirection. If this is true (1) then input or output redirection was performed, and the next command in coms is just a file, so skip over it.
   for(int i=0; i<numComs; ++i)
   {
	   if(in_out_red == 1) //skip over thus iteration if true, as coms[i] here would be the file name used for output or input redirection
	   {
		   in_out_red = 0;
		   continue;
	   }
	
		//pipe, fork, and wait
	   pipe(fd); 
	   pid = fork();
	   wait(&status);
	   if(pid == 0)
	   {
		   if(ops[j] == '<')
		   {
				//This is for input redireaction
			   if(j+1 < numOps) //if this is true, that means there are 1 or more other operations to be performed after the redirection, so this branch duplicates fd[1] to stdout so exec function writes output to write end of pipe.
			   {
				   dup2(fd[1], 1);
			   }
			   char** args = GetArgs(coms[i]);
			   
			   //execute input redirected and exec function stuff here
			   
			   perror(args[0]); //error if exec is unseccessful
			   exit(3); //3 will be the exit code if the child exits because of an exec error
		   }
		   else if(ops[j] == '>')
		   {
			   //This is for output redireaction
			   //NOTE: if there is output redirection, I am pretty sure it would have to be with the last command. Let me know if I am wrong about this.
			   if(prev_p != 0) //check if prev_p is till equal to 0 (filno of stdin) as in the beginning.
			   {
				   //if not, then it's the file descriptor of the read end of the previous pipe, so duplicate it to stdin and close prev_p.
				   dup2(prev_p, 0);
				   close(prev_p);
				   //Now the exec function will get its input (if needed) from the read end of the previous pipe (prev_p) and not stdin.
			   }
			   char** args = GetArgs(coms[i]);
			   
			   //execute command and redirect output to file here.
			   
			    perror(args[0]); //error if exec is unseccessful
				exit(3);
		   }
		   else if(ops[j] == '|')
		   {
				if(i < numComs-1) //This branch is done for the piped commands up until the last command.
				{
					char** args = GetArgs(coms[i]);

					if(prev_p != 0) //check if prev_p is equal to stdin fileno
					{ 	
						//if not, then it's the file descriptor of the read end of the previous pipe, so duplicate it to stdin and close prev_p
						dup2(prev_p, 0);
						close(prev_p);
					}
					dup2(fd[1], 1); //dupicate write end of pipe to stdout, and close write end of pipe
					close(fd[1]);
					execvp(args[0],args); //command now recieves input (if needed) from prev_p and writes output to fd[1]
					
					perror(args[0]);//error if exec is unseccessful
					exit(3);
				}
				else //if the current command is a piped command and its the last command in coms, then its output will go to stdout as normal.
				{
					if(prev_p != 0) //check if prev_p is equal to stdin fileno.
					{
						//if not, then it's the file descriptor of the read end of the previous pipe, so duplicate it to stdin and close prev_p.
						dup2(prev_p, 0);
						close(prev_p);
					}
					
					//NO dupicatation of write end of pipe to stdout. This would be the last command so it's output needs to be written to stdout
					close(fd[1]);
					char** args = GetArgs(coms[i]);
					execvp(args[0],args); //command now recieves input (if needed) from prev_p and writes output to stdout
					
					perror(args[0]); //error if exec is unseccessful
					exit(3);
				}
		   }
	   }
		close(prev_p); //close prev_p so file descriptor can be resused
		close(fd[1]); //close write end of pipe as we are done with it
		prev_p = fd[0]; //assign prev_p with the read end of this pipe so it can be read from in the next pipe
	   
	   if(ops[j] == '<' || ops[j] == '>') //set the bool to 1 if this iteration was an input or output redirection so we can skip over the next iteration as coms[i] for the next iteration would just be the file name.
		in_out_red = 1;
	
	   if(ops[j] == '<') //if this was an input redirection, skip the next operator and go to the following one if there is one. (it doesnt mater whehter the next opertor is a '|' or '>', ouput will be redirected to fd[1] regardless).
	   {
			++j; ++j;
	   }
	   else
	   {
		   ++j; //if it wasn't input redirection, increment to the next opertor (it does not skip).
	   }
	   
	   if(j == numOps && ops[j-1] == '|') //if the last operation is a piped command, add a pipe character to ops so it knows to execute the pipe branch (i.e. ls | wc -----> ls | wc |  , pipe for each command).
	   {
		   ops = (char*)realloc(ops,numOps+1);
		   ++numOps;
		   ops[j] = '|';
	   }
	   if(WEXITSTATUS(status) == 3) //if any error occured, close everythng and exit.
		{
			close(fd[0]);
			close(prev_p);
			close(fd[1]);
			return;
		}
   }
   //when finished, close everythig and exit.
	close(fd[0]);
	close(prev_p);
	close(fd[1]);
	return;
   
}

void cd_run( char** args )
{
	//printf("cd_running \n");
	int MAX = 300;
	if(args[1]==NULL)
	{
		chdir("/home");
		//printf("I should be going home");
		//execlp("pwd", "pwd", NULL);
		return;
		
	}
	
	char** temp = &args[1];
	char pathway[MAX];
	
	if (strcmp(temp[0], "/") != 0)
	{
		getcwd(pathway, sizeof(pathway));
		strcat(pathway, "/");
	}
	
	strcat(pathway, *temp);
	if(chdir(pathway) == -1)
	{
		perror("Error");
		return;
	}
}

void RemoveWhitspace(char** str)
{
	//Removes leading whitespace
	while(isspace(*str[0]))
	{
		++(*str);
	}
	
	//Removes trailing whitespace
	char* end = *str + strlen(*str)-1;
	while(end > *str && isspace(end[0]))
	{
		--end;
	}
	end[1] = '\0';
}

void add_history( const char *cmd )
{
		// loop through commands and add them into the pointer array
		if (count < 20) 
		{
            history[count++] = strdup( cmd );
		} 
		else 
		{
            free( history[0] );
            for (unsigned index = 1; index < 20; index++) 
            {
                history[index - 1] = history[index];
            }
            history[20 - 1] = strdup( cmd );
		}
		
}

// delete the items of the array and assign NULL
void delete_history()
{
		
	memset(history, 0, sizeof(history));
		
}

// loop through the array of commands and print it to the console
void print_history()
{
	for (int i=0; i<count; i++)
	{
		printf("%d %s\n", i+1, history[i]);
	}
	
}

void signal_handler(int signal)
{
	static int terminate;
	
	// if signal is SIGINT (Ctrl+c) it will kill the child process
	if (signal == SIGINT) 
	{
		//print message for the user if it is successfull
		printf("\nCtrl+C received! Killing the child process.\n");
		kill(pid, SIGKILL);
		printf("Done\n");
		exit(0);
	}
	// if signal is Ctrl+z then it will take this route
	if (signal == SIGTSTP) 
	{
		// print the child process id 
		printf ("\nChild PID: %d\n", pid);
		if (!terminate) 
		{
			// print the necessary message if the process is successfull
			printf("\nCtrl+Z received! Stopping the child process.\n");
			kill(pid, SIGSTOP);
			terminate = 1;
		} 
		else 
		{
			// if the signal caught again it will resume the child process
			printf("\nCtrl+Z received! Resuming child process\n");
			kill(pid, SIGCONT);
			terminate = 0;
		}
	}
}



/*
void PipedCommands(char* line)
{
	char** coms = (char**)malloc(sizeof(char*));
	int numComs;
	char delim[] = "|";
	
	int i=0;
	coms[i] = strtok(line, delim);
   while(coms[i] != NULL)
   {
		++i;
		coms = (char**)realloc(coms, sizeof(char*) * (i+1));
		coms[i] = strtok(NULL, delim);
   }
   numComs = i;
   
   int fd[2];
   pid_t pid;
   int status;
   int prev_p = 0; //read end of previous pipe
	
  // printf("\nnumComs: %d\n", numComs);
	for(int i=0; i<numComs; ++i)
	{
		//printf("\ni: %d\n", i);
		pipe(fd);
		pid = fork();
		wait(&status);
		if(pid == 0)
		{
			//printf("\nChild\n");
			if(i < numComs-1)
			{
				char** args = GetArgs(coms[i]);

				if(prev_p != 0) //check if prev_p is equal to stdin fileno
				{ 	
					//if not, duplicate it to stdin and close prev_p
					dup2(prev_p, 0);
					close(prev_p);
				}
				dup2(fd[1], 1); //dupicate write end of pipe to stdout, and close write end of pipe
				close(fd[1]);
				execvp(args[0],args); //command now recieves input (if needed) from prev_p and writes output to fd[1]
				perror(args[0]);
				exit(3);
			}
			else
			{
				if(prev_p != 0) //check if prev_p is equal to stdin fileno
				{
					//if not, duplicate it to stdin and close prev_p
					dup2(prev_p, 0);
					close(prev_p);
				}
				
				//NO dupicatation of write end of pipe to stdout. This would be the last command so it's output needs to be written to stdout
				close(fd[1]);
				char** args = GetArgs(coms[i]);
				printf("\n");
				execvp(args[0],args); //command now recieves input (if needed) from prev_p and writes output to stdout
				perror(args[0]);
				exit(3);
			}
		}
		close(prev_p); //close prev_p so file descriptor can be resused
		close(fd[1]); //close write end of pipe as we are done with it
		prev_p = fd[0]; //assign prev_p with the read end of this pipe so it can be read from in the next pipe
		if(WEXITSTATUS(status) == 3)
		{
			close(fd[0]);
			close(prev_p);
			close(fd[1]);
			return;
		}
	}
	close(fd[0]);
	close(prev_p);
	close(fd[1]);
		
}
*/

