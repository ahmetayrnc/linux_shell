# linux_shell
A simple shell program, a command line interpreter, called bilshell.
It can work in two modes: interactive mode and batch mode.

In interactive mode the shell will provide a prompt string, bilshell-$:, to the user where the user will type a command name, i.e., a program name, with zero or more parameter strings and the shell will execute the command.

In batch mode the shell will read the commands from a file and will execute them one after another.
    An example invocation of the program can be: bilshell 1 infile.txt
    
The shell also supports composition of two commands where the output of one command is given to another command as input. For example, there is “ps aux” program in Linux that is listing the current processes, and there is “sort” program in Linux that is sorting a text file. When we write “ps aux | sort” in the shell, it prints to the screen the sorted list of processes.
