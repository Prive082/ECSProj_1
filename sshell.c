#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16

// implement linked lists so series of commands may interact
struct inputCmd
{
        char *args[MAX_ARGS];
        char *prevCmdInput;
        char *cmdOutput;
        int appendYN;
        struct inputCmd *next;
};

struct Jobs
{
        int pids[MAX_ARGS];
        int exitStats[MAX_ARGS];
        int numProcess;
        char *cmd;
        int wait;
        struct Jobs *next;
        struct Jobs *prev;
};

struct Jobs* initJobs()
{
        struct Jobs *jobs = (struct Jobs*)malloc(sizeof(struct Jobs));
        jobs->next = NULL;
        jobs->prev = NULL;
        jobs->cmd = NULL;
        jobs->numProcess = 0;
        jobs->wait = 0;
        for (int i = 0; i < MAX_ARGS; i++) {
                // Test if the child has exited or not
                jobs->exitStats[i] = -5;
        }
        return jobs;
}

/**
 * Free any single job node in a linked list of jobs
 * If the job is in the middle of the linked list, link the previous and next node together
*/
void freeJob(struct Jobs *job) {
        if (job->prev && job->next) 
        {
                job->prev->next = job->next;
                job->next->prev = job->prev;
        }
        else if (job->prev)
        {
                job->prev->next = NULL;
        }
        else if (job->next) 
        {
                job->next->prev = NULL;
        }
        free(job->cmd);
        free(job);
}

int parseCommand(struct inputCmd *headCmd, char *cmdCopy)
{
        struct inputCmd *currentCmd = headCmd;
        // detect first or last piping operator without operand
        if (cmdCopy[0] == '|' || cmdCopy[strlen(cmdCopy) - 1] == '|')
        {
                fprintf(stderr, "Error: missing command\n");
                return -1;
        }

        // First stage: divide the command up through pipeline operator
        char *pipeCommands[MAX_ARGS];
        int pipe_i = 0;
        char *singleCommand = strtok(cmdCopy, "|");
        while (singleCommand) 
        {
                pipeCommands[pipe_i++] = singleCommand;
                singleCommand = strtok(NULL, "|");
        }

        for (int i = 0; i < pipe_i; i++) {
                char *process = pipeCommands[i];
                char *redirect = strpbrk(process, ">");
                char *background = strpbrk(process, "&");
                
                if (i < pipe_i - 1)
                {
                        // Detect mislocated output and redirection sign
                        if (redirect)
                        {
                                
                                fprintf(stderr, "Error: mislocated output redirection\n");
                                return -1;
                        }
                        if (background)
                        {
                                fprintf(stderr, "Error: mislocated background sign\n");
                                return -1;
                        }

                        // Second stage: Divide each single process command into individual arguments
                        char *argument = strtok(process, " ");
                        int args_i = 0;
                        while (argument)
                        {
                                currentCmd->args[args_i++] = argument;
                                argument = strtok(NULL, " ");
                        }
                        currentCmd->args[args_i] = NULL;
                        currentCmd->next = (struct inputCmd *)malloc(sizeof(struct inputCmd));
                        currentCmd = currentCmd->next;
                        currentCmd->cmdOutput = NULL;
                        currentCmd->prevCmdInput = NULL;
                        currentCmd->next = NULL;
                        currentCmd->appendYN = 0;
                }
                // The last command in the pipeline would contain background sign or redirect sign
                else 
                {
                        if (background) {
                                // Valid background sign need to be in the end of the argument
                                if (process[strlen(process) - 1] != '&')
                                {
                                        fprintf(stderr, "Error: mislocated background sign\n");
                                        return -1;
                                }
                                // If background sign valid, trim out the background sign
                                else {
                                        process[strlen(process) - 1] = '\0';
                                }
                        }
                        if (redirect) {
                                // Detect append sign
                                if (*(redirect+1) == '>') 
                                {
                                        currentCmd->appendYN = 1;
                                }

                                // Seperate command with the redirect file
                                char *command = strtok(process, ">");
                                char *file = strtok(NULL, ">");

                                // if no entry after > sign
                                if (file == NULL) {
                                        fprintf(stderr, "Error: no output file\n");
                                        return -1;
                                }

                                file = strtok(file, " ");

                                // if entry after > is whitespace
                                if (file == NULL) {
                                        fprintf(stderr, "Error: no output file\n");
                                        return -1;
                                }
                                
                                // Divide the command into individual arguments
                                char *argument = strtok(command, " ");
                                int args_i = 0;
                                while (argument)
                                {
                                        currentCmd->args[args_i++] = argument;
                                        argument = strtok(NULL, " ");
                                }
                                currentCmd->args[args_i] = NULL;
                                currentCmd->cmdOutput = strdup(file);

                                // Test if file can be opened
                                int fd = open(currentCmd->cmdOutput, O_WRONLY | O_CREAT, 0644);

                                if (currentCmd->cmdOutput != NULL && fd == -1)
                                {
                                        fprintf(stderr, "Error: cannot open output file\n");
                                        return -1;
                                }
                                close(fd);
                                
                        }
                        // if there's no redirect, simply treat process as line of arguments
                        else 
                        {
                                char *argument = strtok(process, " ");
                                int args_i = 0;
                                while (argument)
                                {
                                        currentCmd->args[args_i++] = argument;
                                        argument = strtok(NULL, " ");
                                }
                                currentCmd->args[args_i] = NULL;
                                currentCmd->next = NULL;
                        }
                }
                
        }
        return 0;
        
}

static void redir(struct inputCmd *storeCmd)
{

        if (storeCmd->cmdOutput != NULL)
        {
                if (storeCmd->appendYN == 0)
                {
                        int fd = open(storeCmd->cmdOutput, O_WRONLY | O_TRUNC);
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                }
                else if (storeCmd->appendYN == 1)
                {
                        int fd = open(storeCmd->cmdOutput, O_WRONLY | O_APPEND);
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                }
        }
}

void freeLinked(struct inputCmd *head)
{
        // strndup and strdup both allocate memory and need to be freed
        if (head->prevCmdInput != NULL)
                free(head->prevCmdInput);
        else if (head->cmdOutput != NULL)
                free(head->cmdOutput);

        struct inputCmd *next = head->next;

        while (next)
        {
                struct inputCmd *tmp = next;
                next = next->next;

                if (tmp->prevCmdInput != NULL)
                        free(tmp->prevCmdInput);
                if (tmp->cmdOutput != NULL)
                        free(tmp->cmdOutput);

                free(tmp);
        }
}

// reference:
// https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
// We refence the beginning stages to create pultiple pipes given an unknown amount of cmds
int pipeCount(struct inputCmd *headCmd)
{
        struct inputCmd *currentCmd = headCmd;
        int count = 0;
        while (currentCmd != NULL)
        {
                count++;
                currentCmd = currentCmd->next;
        }
        return count - 1;
}

void printStatus(char *cmd, int statusArr[], int statusArrLen) 
{
        fprintf(stderr, "+ completed '%s' ", cmd);

        for (int j = 0; j < statusArrLen; j++)
        {
                fprintf(stderr, "[%d]", statusArr[j]);
        }
        fprintf(stderr, "\n");
}

void bgJobHandling(struct Jobs *jobs)
{
        
        struct Jobs *jobsTail = jobs->prev;
        while (jobsTail)
        {
                int status;
                int allComplete = 1;
                for (int i = 0; i < jobsTail->numProcess; i++)
                {
                        int isComplete = waitpid(jobsTail->pids[i], &status, WNOHANG);
                        if (isComplete)
                        {
                                jobsTail->exitStats[i] = WEXITSTATUS(status);
                        }
                        if (jobsTail->exitStats[i] == -5) {
                                allComplete = 0;
                        }
                }
                struct Jobs* tempPrevJobHolder = jobsTail->prev;
                if (allComplete) {
                        printStatus(jobsTail->cmd, jobsTail->exitStats, jobsTail->numProcess);
                        freeJob(jobsTail);
                }
                jobsTail = tempPrevJobHolder;
        }        
}

int pipeExecute(struct inputCmd *head, char *cmd, struct Jobs *jobs)
{
        struct inputCmd *currentCmd = head;
        int numPipes = pipeCount(head);
        int status = EXIT_SUCCESS;
        int pipeFds[numPipes * 2];
        pid_t pid;
        jobs->cmd = (char*)malloc(sizeof(char) * strlen(cmd));
        strcpy(jobs->cmd, cmd);

        for (int i = 0; i < numPipes; i++)
        {
                if (pipe(pipeFds + i * 2) < 0)
                {
                        perror("Pipe creation:");
                        exit(1);
                }
        }

        int cmdCounter = 0;
        while (currentCmd != NULL)
        {
                pid = fork();
                // Child process
                if (pid == 0)
                {
                        redir(currentCmd);
                        // if not last command
                        if (currentCmd->next)
                        {
                                if (dup2(pipeFds[cmdCounter + 1], STDOUT_FILENO) < 0)
                                {
                                        perror("dup2");
                                        exit(EXIT_FAILURE);
                                }
                        }

                        // if not first command&& cmdCounter!= 2*numPipes
                        if (cmdCounter != 0)
                        {
                                if (dup2(pipeFds[cmdCounter - 2], STDIN_FILENO) < 0)
                                {
                                        perror("dup2"); /// cmdCounter-2 0 cmdCounter+1 1
                                        exit(EXIT_FAILURE);
                                }
                        }

                        for (int i = 0; i < 2 * numPipes; i++)
                        {
                                close(pipeFds[i]);
                        }

                        if (execvp(currentCmd->args[0], currentCmd->args) < 0)
                        {
                                fprintf(stderr, "Error: command not found\n");
                                exit(1);
                        }
                }
                else if (pid < 0)
                {
                        perror("error");
                        exit(EXIT_FAILURE);
                }

                // Parent process
                // Keep track of pids of the child processes in the order of their pipeline
                jobs->pids[jobs->numProcess++] = pid;

                currentCmd = currentCmd->next;
                cmdCounter += 2;
        }
        for (int i = 0; i < 2 * numPipes; i++)
        {
                close(pipeFds[i]);
        }
        if (strpbrk(cmd, "&") != NULL)
        {
                jobs->wait = 1;
                bgJobHandling(jobs);
        }
        else
        {
                for (int i = 0; i < jobs->numProcess; i++)
                {
                        waitpid(jobs->pids[i], &status, 0);
                        jobs->exitStats[i] = WEXITSTATUS(status);
                }
                bgJobHandling(jobs);
                printStatus(cmd, jobs->exitStats, jobs->numProcess);
        }
        return 0;
}

int main(void)
{
        // implement cmd as struct for easier data manipulation
        char cmd[CMDLINE_MAX];
        putenv("/bin");
        struct Jobs *jobs = initJobs();

        while (1)
        {
                // consider this our head node
                struct inputCmd storeCmd;
                storeCmd.appendYN = 0;
                storeCmd.prevCmdInput = NULL;
                storeCmd.cmdOutput = NULL;
                storeCmd.next = NULL;

                char *nl;
                // int retval;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO))
                {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

                /* Builtin command */
                if (strlen(cmd) == 0) {
                        bgJobHandling(jobs);
                        continue;
                }
                if (!strcmp(cmd, "exit"))
                {
                        bgJobHandling(jobs);
                        if (jobs->prev) {
                                fprintf(stderr, "Error: active jobs still running\n");
                                continue;
                        }
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                        break;
                }

                // Make string copy for parsing
                char cmdCopy[CMDLINE_MAX];
                strcpy(cmdCopy, cmd);
                // parse the copy string
                if(parseCommand(&storeCmd, cmdCopy)== -1){
                        freeLinked(&storeCmd);
                        continue;
                }

                /* Regular command */

                /*retval = system(cmd);
                fprintf(stdout, "Return status value for '%s': %d\n",
                        cmd, retval);*/
                if (strcmp(storeCmd.args[0], "pwd") == 0)
                {
                        bgJobHandling(jobs);
                        char buff[CMDLINE_MAX];
                        if (getcwd(buff, CMDLINE_MAX) == buff)
                        {
                                printf("%s\n", buff);
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);

                        }
                        else
                        {
                                perror("getcwd");
                        }
                }
                else if (strcmp(storeCmd.args[0], "cd") == 0)
                {
                        bgJobHandling(jobs);
                        int result = chdir(storeCmd.args[1]);
                        if (result == 0)
                        {
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);

                        }
                        else
                        {
                                fprintf(stderr, "Error: cannot cd into directory\n");
                        }
                }
                else
                {
                        pipeExecute(&storeCmd, cmd, jobs);
                        freeLinked(&storeCmd);
                }

                jobs->next = initJobs();
                jobs->next->prev = jobs;
                jobs = jobs->next;
                if (jobs->prev->wait) 
                {
                        continue;
                }
                else 
                {
                        freeJob(jobs->prev);
                }

        }

        return EXIT_SUCCESS;
}
