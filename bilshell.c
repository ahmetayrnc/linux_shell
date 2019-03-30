#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

//for builtin functions
int bilshell_quit(char **args);

//names of the builtin functions
char *builtin_str[] = {
    "quit"};

int (*builtin_func[])(char **) = {
    &bilshell_quit};

//number of builtin functions
int bilshell_num_builtins()
{
    return sizeof(builtin_str) / sizeof(char *);
}

//quit function
int bilshell_quit(char **args)
{
    printf("Byeeee\n");
    return 0;
}

//gets a line
char *read_line(int from_file, FILE *fptr, int *line_status)
{
    char *line = NULL;
    ssize_t bufsize = 0;
    if (from_file == 1) //read from file
    {
        *line_status = getline(&line, &bufsize, fptr);
    }
    else //read from stdin
    {
        getline(&line, &bufsize, stdin);
    }

    return line;
}

//return 0 if no pipe, 1 if piped
int isPiped(char *str, char **str_piped)
{
    int i;
    for (i = 0; i < 2; i++)
    {
        // seperate the arguments into two if there is a pipe
        str_piped[i] = strsep(&str, "|");
        // if there is no pipe just break
        if (str_piped[i] == NULL)
        {
            break;
        }
    }

    // return 0 if there is no piped command
    if (str_piped[1] == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

//splits the line into tokens
#define TOKEN_BUFFER_SIZE 64
#define TOKEN_DELIM " \t\r\n\a"
char **get_tokens(char *line)
{
    int bufferSize = TOKEN_BUFFER_SIZE;
    int position = 0;
    char **tokens = malloc(bufferSize * sizeof(char *));
    char *token;

    //if there is a problem with the buffer exit with error
    if (!tokens)
    {
        fprintf(stderr, "bilshell: allocation error\n");
        exit(-1);
    }

    //take a token
    token = strtok(line, TOKEN_DELIM);
    while (token != NULL)
    {
        //place the token and increase the position
        tokens[position] = token;
        position++;

        if (position >= bufferSize)
        {
            //if we fill the buffer increase its size
            bufferSize += TOKEN_BUFFER_SIZE;
            tokens = realloc(tokens, bufferSize * sizeof(char *));

            //if there is a problem with the buffer exit with error
            if (!tokens)
            {
                fprintf(stderr, "bilshell: allocation error\n");
                exit(-1);
            }
        }

        //get a new token
        token = strtok(NULL, TOKEN_DELIM);
    }
    //when we run out of tokens to get place null at the end
    tokens[position] = NULL;
    return tokens;
}

//return 1 for regular, 2 for piped
int process_line(char *line, char ***args1, char ***args2)
{
    char *str_piped[2]; //char* for possibly two sets of arguments
    int piped = 0;      //if the command is piped or not

    //check if the command is piped and if it is
    //fill str_piped
    piped = isPiped(line, str_piped);

    if (piped)
    {
        //put the arguments into their places and return 2 for piped
        *args1 = get_tokens(str_piped[0]);
        *args2 = get_tokens(str_piped[1]);
        return 2;
    }
    else
    {
        //put the arguments into their place and return 1 for regular
        *args1 = get_tokens(line);
        return 1;
    }
}

//launch a linux command
int bilshell_launch(char **args)
{
    pid_t pid;  //process id
    pid_t wpid; //process id to wait
    int status; //status of wait

    pid = fork();

    //if can't fork
    if (pid < 0)
    {
        printf("bilshell: Could not fork child 1");
        exit(-1);
    }

    if (pid == 0) //child process
    {
        //execute the command check for errors
        if (execvp(args[0], args) == -1)
        {
            printf("bilshell: Could not exec child 1");
        }
        exit(-1);
    }
    else //parent process
    {
        do
        {
            //wait for child to finish
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

#define BUFFER_SIZE_PIPE 25
#define READ_END 0
#define WRITE_END 1
int execute_piped(char **args1, char **args2, int buffer_size)
{
    int pipefd1[2];
    int pipefd2[2];
    pid_t p1;
    pid_t p2;

    int char_count = 0;
    int read_call_count = 0;

    //if can't create first pipe
    if (pipe(pipefd1) < 0)
    {
        printf("bilshell: Could not create pipe 1");
        exit(-1);
    }

    fcntl(pipefd1[0], F_SETPIPE_SZ, 1024 * 1024);
    fcntl(pipefd1[1], F_SETPIPE_SZ, 1024 * 1024);

    //if can't fork 1st child
    p1 = fork();
    if (p1 < 0)
    {
        printf("bilshell: Could not fork child 1");
        exit(-1);
    }

    if (p1 == 0) // Child 1 process
    {
        //create a duplicate for the write end
        dup2(pipefd1[WRITE_END], 1);

        //if can't exec the 1st child
        if (execvp(args1[0], args1) < 0)
        {
            printf("bilshell: Could not exec child 1");
            exit(-1);
        }
    }
    else // Parent process
    {
        //wait for the first child to finish
        wait(NULL);

        //close the write end of 1st pipe
        close(pipefd1[WRITE_END]);

        //if can't create 2nd pipe
        if (pipe(pipefd2) < 0)
        {
            printf("bilshell: Could not create pipe 2");
            exit(-1);
        }

        fcntl(pipefd2[0], F_SETPIPE_SZ, 1024 * 1024);
        fcntl(pipefd2[1], F_SETPIPE_SZ, 1024 * 1024);

        //read from the first pipe and write to the second pipe
        ssize_t read_size;
        char *buffer[buffer_size];
        while ((read_size = read(pipefd1[0], buffer, buffer_size)) > 0)
        {
            char_count += read_size;
            read_call_count++;
            write(pipefd2[1], buffer, read_size);
        }

        //close the ends of pipes we used
        close(pipefd1[READ_END]);
        close(pipefd2[WRITE_END]);

        //if can't fork 2nd child
        p2 = fork();
        if (p2 < 0)
        {
            printf("bilshell: Could not fork child 2");
            exit(-1);
        }

        if (p2 == 0) // Child 2 process
        {
            //create a duplicate for the read end
            dup2(pipefd2[READ_END], 0);

            //if can't exec 2nd child
            if (execvp(args2[0], args2) < 0)
            {
                printf("bilshell: Could not exec child 2");
                exit(-1);
            }
        }
        else // Parent process
        {
            //wait for the 2nd child to finish
            wait(NULL);

            //close the read end of the 2nd pipe
            close(pipefd2[READ_END]);

            printf("character-count: %d\n", char_count);
            printf("read-call-count: %d\n", read_call_count);
        }
    }
    return 1;
}

//execute a command, linux or builtin
int execute(char **args)
{
    int i;

    //empty command
    if (args[0] == NULL)
    {
        return 1;
    }

    //if it finds a built in command executes it
    for (i = 0; i < bilshell_num_builtins(); i++)
    {
        if (strcmp(args[0], builtin_str[i]) == 0)
        {
            return (*builtin_func[i])(args);
        }
    }

    //else executes linux command
    return bilshell_launch(args);
}

void command_loop(int from_file, FILE *fptr, int buffer_size)
{
    char *line;           //line we are reading
    char **args1;         //arguments we will get from the lines
    char **args2;         //arguments we will get from the lines if pipe
    int status;           //status so we know when to finish the loop
    int command_type = 0; //command type (piped | regular)
    int line_status;      //line status (to detect eof)

    do
    {
        if (from_file != 1)
        {
            printf("bilshell-$:"); //the prompt
        }
        line = read_line(from_file, fptr, &line_status); //read a line

        //if the line is problematic (eof) break the loop
        if (line_status == -1)
        {
            free(line); //frees the line
            printf("Byeeee\n");
            break;
        }

        command_type = process_line(line, &args1, &args2); //find the command type

        if (command_type == 1) //regular command
        {
            status = execute(args1);
        }
        else if (command_type == 2) //piped command
        {
            status = execute_piped(args1, args2, buffer_size);
            free(args2); //frees the arguments2
        }

        free(line);  //frees the line
        free(args1); //frees the arguments1
    } while (status);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        return 0;
    }

    int buffer_length = atoi(argv[1]);

    //if there is no file specified run in interactive mode
    if (argc == 2) //interactive mode
    {
        //greet the user and run the loop in interactive
        printf("\033[H\033[J");
        char *username = getenv("USER");
        printf("Helloooooo ");
        printf("%s\n", username);
        command_loop(0, NULL, buffer_length);
    }
    else if (argc == 3) //batch mode
    {
        printf("Batch Mode!\n");

        //open the file and run the loop in batch mode
        char c[1024];
        FILE *fptr;
        if ((fptr = fopen(argv[2], "r")) == NULL)
        {
            printf("Error! opening file\n");
            exit(1);
        }
        command_loop(1, fptr, buffer_length);
        fclose(fptr);
    }

    return 0;
}