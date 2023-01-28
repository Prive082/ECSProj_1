# SSHELL: Simple Shell

## Summary

The goal of this project is to understand important UNIX system calls by 
implementing a simple shell called **sshell**. A shell is a command-line 
interpreter: it accepts input as command lines while executing as jobs.

In the following example, the shell is in charge of printing the 
*shell prompt*, understanding the user-inputted command line (redirect the 
output of executable `echo` with arguments `hello` and `world` to the file 
`file.txt`), and executing it as well as waiting for completion before
prompting for new user-intput (run executable `cat` to display contents of
`file.txt`). 

```console
prive082@DESKTOP-CU7BBBB:/mnt/c/Users/prive/Desktop/ECS Code/ECS150/p1$ 
echo hello world > file.txt
prive082@DESKTOP-CU7BBBB:/mnt/c/Users/prive/Desktop/ECS Code/ECS150/p1$ 
cat file.txt 
hello world
```

## Implementation

Our simple shell will have similar functionality to shells such as *bash* and
*zsh*. The shell will be able to:

1. execute user-supplied commands with optional arguments
2. offer a selection of builtin commands
3. redirect the standard output of commands to files
4. compose and execute commands via pipelining
5. append the redirected standard output of commands to files
6. be able to run background jobs

### Executing Simple Commands

At the beginning of its execution, `sshell` creates and initializes 2 empty
data structures, ` struct inputCmd storeCmd` (linked list of processes) and
`struct Jobs jobs` (linked list of jobs). 

Upon prompting the user to input a command, `sshell` stores the command into
a string, and copies the command into a separate string before calling
function `parse Command()`. This is done to preserve the original command as
`parseCommand()` will alter the string permenantly. 

Function `parseCommand()` runs through the string using `strtok()`, which 
copies a string into a token string until specifically a *' '* char is found.
After a token is found, `strpbrk()` is called which returns the first char 
found in our token that is also found in string *">|&"*. However, simple
commands will always result in `NULL` when passed into `strpbrk()`, so all
that will happen is it runs into **else** which assigns each parsed item from
`strtok()` into `char *args[MAX_ARGS];` from the passed in `&storeCmd`.

Upon completion it runs function `pipeExecute()`, which for a simple command
uses none of the piping featues and only executes. The exit message is then printed.

``` console
+ completed 'cmd' [0]
```

Once back to main, sshell prompts another user-input command. 

### Offer Builtin Commands

Separated from the executable commands are the Builtin commands ran only by `sshell`.

For command *pwd*: `sshell` checks if the parsed command is *"pwd"*, if so
get the value of the current directory and store in a string to print. 
Failure to get the directory results in error.

For command *cd*: `sshell` checks if the parsed arg is *"cd"*, run change directory function with the file as parameter. Failure results in error message.

For command *exit*: `sshell` checks if the parsed arg is *"exit"*, run change directory function with the file as parameter. Failure results in error message.

### Redirect Standard Output
Upon prompting the user to input a command, `sshell` runs a redirect command
near identical to a simple command. However, in function `parseCommand()`, 
`strpbrk()` is no longer **NULL** due to the **>** character. Once assigned as 
no longer **NULL**,  function `parseRedir()` is called.

Function ``parseRedir()`` will isolate any arguments prior to the **>**
character and add them to `char *args[MAX_ARGS];` from `&storeCmd`. Afterwards
it will take the file input after the **>** and assign it to 
`storeCmd->cmdOutput`.

Upon completion it runs function `pipeExecute()`, which for a redirection 
command uses none of the piping featues and only executes. However, before 
execution ``Redir()`` is called, which will redirect the output from stdout
to the filename stored in `storeCmd->cmdOutput`, either making a new file
or simply redirecting. Upon execution, an exit message is given and `sshell`
prompts a new message. 

### Pipelining
Upon prompting the user to input a command, `sshell` runs a pipelined command
near identical to a simple command. However, in function `parseCommand()`, 
`strpbrk()` is no longer **NULL** due to the **|** and the **>** characters.
Once assigned as no longer **NULL**,  functions `parsePipe()` and 
`parseRedir()` is called.

Function `parsePipe()` separates commands from **|** chars the same as 
`parseRedir()` with **>** chars. However, since we are separating commands,
everything to the left of **|** is assigned to `char *args[MAX_ARGS];` in
`&storeCmd`, and everything to the right of **|** is assigned to 
`char *args[MAX_ARGS];` in `&storeCmd->next`. Using `->next` is how `sshell`
chains together command nodes into a linked list to be used when calling
`pipeExecute()` upon exiting `parseCommand()`. 

Function `pipeExecute()` runs uniquely, as with helper functions 
`pipeCount()`, which based on the number of `&storeCmd` nodes 
determines how many pipes we need. Once all the pipes are created
`sshell` runs a **While** loop which for every command node fork and 
create a child process. It then pipes the output to the read node of pipe, 
and pipe the input to the write node. Lastly it executes the piped command.
Once the pipeline is completed it exits the **While** loop and in the parent
process it `Wait()` until all children are done and return a message with each
exit status. Lastly, in `main()` `sshell` prompts for a new user-input.

``` console
+ completed 'cmd | cmd' [0][0]
```

### Appending

Appending runs identical to redirection, however an added feature was
implemented to `parseRedir()`, which checks if a **>** character exists 
next to the **>** char fount by `strpbrk()`. If this is true we know the 
symbol is **>>** which means to append. Ultimately the process to separate
arguments and the file from **>>** is the same, however we assign 
`int appendYN;` to **1**. `int appendYN;` acts as a flag in `&storeCmd`. 
How it works is when `Redir()` checks if `int appendYN;` is **1**, and if
so we append rather than truncate. That is the only difference in the process.

### Background Jobs

Background works as a linked list (`struct Jobs jobs`) that contains an 
array of the pid (`int pids[MAX_ARGS];`) of the processes generated in a 
linux command, and an array of the exit status of these processes 
(`int exitStats[MAX_ARGS];`). Upon each command line entry, 
a background job node is created at the end of the linked list, 
and whichever job that was determined as completed will have that 
job's node removed. The remaining nodes that are before the current job 
node will be the nodes for the background jobs that 
are still running, which is how the linked list keep track of multiple 
background jobs.

After each command line input, the nodes before the current job's node will 
be traversed and check if all the processes of the job are finished. 
(`bgJobHandling()`) If they are finished, the completion message will be printed, 
and the node of that completed job will be removed from the linked list.






