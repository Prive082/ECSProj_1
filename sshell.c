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

struct Jobs *initJobs()
{
        struct Jobs *jobs = (struct Jobs *)malloc(sizeof(struct Jobs));
        jobs->next = NULL;
        jobs->prev = NULL;
        jobs->cmd = NULL;
        jobs->numProcess = 0;
        jobs->wait = 0;
        for (int i = 0; i < MAX_ARGS; i++)
        {
                // Test if the child has exited or not
                jobs->exitStats[i] = -5;
        }
        return jobs;
}

void freeJob(struct Jobs *job)
{
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

int parsePipe(char **ifMeta, struct inputCmd **currentCmd, char **token, char *metaChar, int *args_i)
{
        // check if we are in instances cmd|cmd ot cmd| cmd
        if (strcmp(*token, metaChar) != 0 && **ifMeta != **token)
        {
                // find position of metachar then only add the command to args list without redirect
                int isMetaChar = strcspn(*token, metaChar);
                (*currentCmd)->args[(*args_i)] = strndup(*token, isMetaChar);
                (*args_i)++;

                // since end of args array, must add null
                (*currentCmd)->args[(*args_i)] = NULL;
                // create and allocate space for new command node and assign as current node
                (*currentCmd)->next = (struct inputCmd *)malloc(sizeof(struct inputCmd));
                (*currentCmd) = (*currentCmd)->next;

                // initialize new cmd node
                (*args_i) = 0;
                (*currentCmd)->cmdOutput = NULL;
                (*currentCmd)->prevCmdInput = NULL;
                (*currentCmd)->next = NULL;
                (*currentCmd)->appendYN = 0;

                // if space char after meta, strtok and copy, else, copy everything after meta
                if (*((*ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        (*currentCmd)->args[*args_i] = strdup(*token);
                        (*args_i)++;
                }
                else
                {
                        (*currentCmd)->args[*args_i] = strdup(*ifMeta + 1);
                        (*args_i)++;
                }
        }
        else
        {
                // since end of args array, must add null
                (*currentCmd)->args[(*args_i)] = NULL;
                // create and allocate space for new command node and assign as current node
                (*currentCmd)->next = (struct inputCmd *)malloc(sizeof(struct inputCmd));
                (*currentCmd) = (*currentCmd)->next;

                // initialize new cmd node
                (*args_i) = 0;
                (*currentCmd)->cmdOutput = NULL;
                (*currentCmd)->prevCmdInput = NULL;
                (*currentCmd)->next = NULL;
                (*currentCmd)->appendYN = 0;

                // if space char after meta, strtok and copy, else, copy everything after meta
                if (*((*ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        (*currentCmd)->args[*args_i] = strdup(*token);
                        (*args_i)++;
                }
                else
                {
                        (*currentCmd)->args[*args_i] = strdup(*ifMeta + 1);
                        (*args_i)++;
                }
        }
        return 0;
}

int parseRedir(char **ifMeta, struct inputCmd **storeCmd, char **token, char *metaChar, int *args_i)
{

        // Check if we are in instance Cmd< input or Cmd<input
        if (strcmp(*token, metaChar) != 0 && **ifMeta != **token)
        {
                // find position of metachar then only add the command to args list without redirect
                int isMetaChar = strcspn(*token, metaChar);
                (*storeCmd)->args[(*args_i)] = strndup(*token, isMetaChar);
                (*args_i)++;

                // if there is a space after metachar, strtok and copy string to output
                if (*(*(ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        if (*token == NULL)
                        {
                                fprintf(stderr, "Error: no output file");
                                return -1;
                        }
                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*token);
                }
                // if we are dealing with append, separate file name from meta chars and copy to output
                else if (*((*ifMeta) + 1) == **ifMeta)
                {
                        (*storeCmd)->appendYN = 1;
                        *token = strtok(NULL, " ");
                        if (*token == NULL)
                        {
                                fprintf(stderr, "Error: no output file");
                                return -1;
                        }
                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*token);
                }
                // if metachar is connected to char, copy all chars after metachar to output
                else
                {

                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*ifMeta + 1);
                }
        }
        // instances cmd <input and Cmd < input
        else
        {
                // if there is a space after metachar, strtok and copy string to args
                if (*(*(ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        if (*token == NULL)
                        {
                                fprintf(stderr, "Error: no output file");
                                return -1;
                        }
                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*token);
                }
                // if we are dealing with append, separate file name from meta chars and copy to output
                else if (*((*ifMeta) + 1) == **ifMeta)
                {
                        (*storeCmd)->appendYN = 1;
                        *token = strtok(NULL, " ");
                        if (*token == NULL)
                        {
                                fprintf(stderr, "Error: no output file");
                                return -1;
                        }
                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*token);
                }
                // if metachar is connected to char, copy all chars after metachar
                else
                {
                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*ifMeta + 1);
                }
        }
        return 0;
}

int parseBG(struct inputCmd **storeCmd, char **token, char *metaChar, int *args_i)
{
        // parse & from arg and add arg to list of args
        char *bgToken = strtok((*token), metaChar);
        (*storeCmd)->args[*args_i] = strdup((bgToken));
        (*args_i)++;
        return 0;
}

int mislocatedRedir(const struct inputCmd *head, const struct inputCmd *current)
{

        // Error: cmd > output | cmd
        if (head->next != NULL && head->cmdOutput != NULL)
        {
                fprintf(stderr, "Error: mislocated output redirection\n");
                return -1;
        }

        // Error: if anything besides final node/ pipe process has output redir
        head = head->next;

        while (head != NULL)
        {
                if (head == current)
                        break;

                if (head->cmdOutput != NULL)
                {
                        fprintf(stderr, "Error: mislocated output redirection\n");
                        return -1;
                }
                head = head->next;
        }
        return 0;
}

int parseCommand(struct inputCmd *headCmd, char *cmdCopy)
{
        struct inputCmd *currentCmd = headCmd;
        int args_i = 0;
        int cmdlen = strlen(cmdCopy) - 1;

        // if & is not the last token of the job, fail case
        if (strpbrk(cmdCopy, "&") && cmdCopy[cmdlen] != '&')
        {
                fprintf(stderr, "Error: mislocated background sign\n");
                return -1;
        }

        // parse job at every space char
        char *token = strtok(cmdCopy, " ");

        while (token != NULL)
        {
                if (args_i > MAX_ARGS - 1)
                {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return -1;
                }
                if (args_i == 0 && (*token == '>' || *token == '|'))
                {
                        fprintf(stderr, "Error: missing command\n");
                        return -1;
                }

                // sets pointer if Redirect or pipeline ot bg job
                char *ifMeta = strpbrk(token, ">|&");
                if (ifMeta != NULL)
                {
                        switch (*ifMeta)
                        {
                        case '>':
                                if (parseRedir(&ifMeta, &currentCmd, &token, ">", &args_i) == -1)
                                {
                                        return -1;
                                }
                                break;
                        case '|':
                                if (parsePipe(&ifMeta, &currentCmd, &token, "|", &args_i) == -1)
                                {
                                        return -1;
                                }
                                break;
                        case '&':
                                if (parseBG(&currentCmd, &token, "&", &args_i) == -1)
                                {
                                        return -1;
                                }
                                break;
                        }
                }
                else
                {
                        // no metachars present
                        currentCmd->args[args_i] = token;
                        args_i++;
                }

                token = strtok(NULL, " ");
        }
        currentCmd->args[args_i] = NULL;

        // check if redirect commands are not mislocated
        if (mislocatedRedir(headCmd, currentCmd) == -1)
                return -1;

        // check if create/check write file works
        int fd = 0;

        fd = open(currentCmd->cmdOutput, O_WRONLY | O_CREAT, 0644);

        if (currentCmd->cmdOutput != NULL && fd == -1)
        {
                fprintf(stderr, "Error: cannot open output file\n");
                return -1;
        }
        return 0;
}

// if an output value exists at a process node, respond and redirect to file
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

// strndup and strdup need to be freed
// we need to free every output that is not null
// this is to prevent mem leak
void freeLinked(struct inputCmd *head)
{

        if (head->cmdOutput != NULL)
                free(head->cmdOutput);

        struct inputCmd *next = head->next;

        while (next)
        {
                struct inputCmd *tmp = next;
                next = next->next;

                if (tmp->cmdOutput != NULL)
                        free(tmp->cmdOutput);

                free(tmp);
        }
}

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

// reference:
// https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-c
// We refence the beginning stages to create multiple pipes given an unknown amount of cmds
int pipeExecute(struct inputCmd *head, char *cmd, struct Jobs *jobs)
{
        // define variables for piping
        struct inputCmd *currentCmd = head;
        int numPipes = pipeCount(head);
        int pipeFds[numPipes * 2];
        pid_t pid;
        int status = EXIT_SUCCESS;

        // store process into current job
        jobs->cmd = (char *)malloc(sizeof(char) * strlen(cmd));
        strcpy(jobs->cmd, cmd);

        // create all pipes beforehand
        // pipeFds + i * 2 is used as every pipe requires 2 ints for stdin and stdout
        for (int i = 0; i < numPipes; i++)
        {
                if (pipe(pipeFds + i * 2) < 0)
                {
                        perror("Pipe creation:");
                        exit(1);
                }
        }

        // create var to keep track of positioning
        int cmdCounter = 0;
        while (currentCmd != NULL)
        {
                pid = fork();
                if (pid == 0)
                {
                        // redir only works when last command
                        redir(currentCmd);
                        // if not last command, point stdout to pipe write
                        if (currentCmd->next)
                        {
                                if (dup2(pipeFds[cmdCounter + 1], STDOUT_FILENO) < 0)
                                {
                                        perror("dup2");
                                        exit(EXIT_FAILURE);
                                }
                        }

                        // if not first command, point stdin to pipe read
                        if (cmdCounter != 0)
                        {
                                if (dup2(pipeFds[cmdCounter - 2], STDIN_FILENO) < 0)
                                {
                                        perror(" dup2");
                                        exit(EXIT_FAILURE);
                                }
                        }

                        for (int i = 0; i < 2 * numPipes; i++)
                        {
                                close(pipeFds[i]);
                        }

                        // execute process and print to stderr if fail
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
        }
        else
        {
                // on normal foreground job:
                // wait for completion of entire pipe
                // Then check if any process passed or failed (if status is not 0) and store to array of exit status
                for (int i = 0; i < jobs->numProcess; i++)
                {
                        waitpid(jobs->pids[i], &status, 0);
                        jobs->exitStats[i] = WEXITSTATUS(status);
                }
                printStatus(cmd, jobs->exitStats, jobs->numProcess);
        }
        return 0;
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
                        if (jobsTail->exitStats[i] == -5)
                        {
                                allComplete = 0;
                        }
                }
                struct Jobs *tempPrevJobHolder = jobsTail->prev;
                if (allComplete)
                {
                        printStatus(jobsTail->cmd, jobsTail->exitStats, jobsTail->numProcess);
                        freeJob(jobsTail);
                }
                jobsTail = tempPrevJobHolder;
        }
}

int main(void)
{
        // implement cmd as struct for easier data manipulation
        char cmd[CMDLINE_MAX];
        putenv("/bin");
        struct Jobs *jobs = initJobs();

        while (1)
        {
                // consider this our head node and define all elements of head
                struct inputCmd storeCmd;
                storeCmd.appendYN = 0;
                storeCmd.prevCmdInput = NULL;
                storeCmd.cmdOutput = NULL;
                storeCmd.next = NULL;

                char *nl;

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

                bgJobHandling(jobs);

                /* Builtin command */
                if (strlen(cmd) == 0)
                {
                        bgJobHandling(jobs);
                        continue;
                }
                if (!strcmp(cmd, "exit"))
                {
                        if (jobs->prev)
                        {
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
                // failure to parse will take us directly to the next job
                if (parseCommand(&storeCmd, cmdCopy) == -1)
                {
                        freeLinked(&storeCmd);
                        continue;
                }

                /* Regular command */
                // check if arg is "pwd", if so get the value and store in a string to print
                // failure to get the directory results in error
                if (strcmp(storeCmd.args[0], "pwd") == 0)
                {
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
                // check if arg is cd
                // run change directory function with the file as parameter
                // failure results in error message
                else if (strcmp(storeCmd.args[0], "cd") == 0)
                {
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
                // all normal commands must be executed and freed to prevent any segfaults
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
