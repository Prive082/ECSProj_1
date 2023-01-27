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

int parsePipe(char **ifMeta, struct inputCmd **currentCmd, char **token, char *metaChar, int *args_i)
{
        if (strcmp(*token, metaChar) != 0 && **ifMeta != **token)
        {
                // find position of metachar then only add the command to args list without redirect
                int isMetaChar = strcspn(*token, metaChar);
                (*currentCmd)->args[(*args_i)] = strndup(*token, isMetaChar);
                //printf("Arg [%d]: %s\n", (*args_i), (*currentCmd)->args[(*args_i)]);
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

                //printf("Token: %s\n", *token);
                if (*((*ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        (*currentCmd)->args[*args_i] = strdup(*token);
                        //printf("Arg [%d]: %s\n", (*args_i), (*currentCmd)->args[(*args_i)]);
                        (*args_i)++;
                }
                else
                {
                        (*currentCmd)->args[*args_i] = strdup(*ifMeta + 1);
                        //printf("Arg [%d]: %s\n", (*args_i), (*currentCmd)->args[(*args_i)]);
                        (*args_i)++;
                }
                // Check if we are in instance Cmd < input or Cmd <input
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

                //printf("Token: %s\n", *token);
                if (*((*ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        (*currentCmd)->args[*args_i] = strdup(*token);
                        //printf("Arg [%d]: %s\n", (*args_i), (*currentCmd)->args[(*args_i)]);
                        (*args_i)++;
                }
                else
                {
                        (*currentCmd)->args[*args_i] = strdup(*ifMeta + 1);
                        //printf("Arg [%d]: %s\n", (*args_i), (*currentCmd)->args[(*args_i)]);
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
                //printf("Arg [%d]: %s\n", (*args_i), (*storeCmd)->args[(*args_i)]);
                (*args_i)++;

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
                else
                {

                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*ifMeta + 1);
                }
                // Check if we are in instance Cmd < input or Cmd <input
        }
        else
        {
                //*token = strtok(NULL, " ");

                if (*(*(ifMeta) + 1) == '\0')
                {
                        *token = strtok(NULL, " ");
                        //printf("here\n");
                        if (*token == NULL)
                        {
                                fprintf(stderr, "Error: no output file");
                                return -1;
                        }
                        if (*metaChar == '>')
                                (*storeCmd)->cmdOutput = strdup(*token);
                }
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
        char *bgToken = strtok((*token), metaChar);
        (*storeCmd)->args[*args_i] = strdup((bgToken));
        //printf("Arg [%d]: %s\n", (*args_i), (*storeCmd)->args[(*args_i)]);
        (*args_i)++;
        return 0;
}

int mislocated(const struct inputCmd *head, const struct inputCmd *tail)
{

        // Error: cmd > output | cmd, cmd| cmd < input
        if (head->next != NULL && head->cmdOutput != NULL)
        {
                fprintf(stderr, "Error: mislocated output redirection\n");
                return -1;
        }

        if (tail->prevCmdInput != NULL && tail != head)
        {
                fprintf(stderr, "Error: mislocated input redirection\n");
                return -1;
        }

        // Error cmd | cmd > cmd | cmd , cmd | cmd < cmd | cmd
        head = head->next;

        while (head != NULL)
        {
                if (head == tail)
                        break;

                if (head->cmdOutput != NULL)
                {
                        fprintf(stderr, "Error: mislocated output redirection\n");
                        return -1;
                }
                else if (head->prevCmdInput != NULL)
                {
                        fprintf(stderr, "Error: mislocated input redirection\n");
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
        if (strpbrk(cmdCopy, "&") && cmdCopy[cmdlen] != '&')
        {
                fprintf(stderr, "Error: mislocated background sign\n");
                return -1;
        }
        //printf("Whole Command: %s\n", cmdCopy);
        char *token = strtok(cmdCopy, " ");
        //printf("Token: %s\n", token);
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
                // sets pointer if Redirect or pipeline
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
                { // no metachars present
                        currentCmd->args[args_i] = token;
                        //printf("Arg [%d]: %s\n", args_i, currentCmd->args[args_i]);
                        args_i++;
                }
                token = strtok(NULL, " ");
                //printf("Token: %s\n", token);
        }
        currentCmd->args[args_i] = NULL;

        if (mislocated(headCmd, currentCmd) == -1)
                return -1;
        // check if read works and create/check write file
        int fd = 0;

        fd = open(currentCmd->cmdOutput, O_WRONLY | O_CREAT, 0644);

        if (currentCmd->cmdOutput != NULL && fd == -1)
        {
                fprintf(stderr, "Error: cannot open output file\n");
                return -1;
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

int pipeExecute(struct inputCmd *head, char *cmd)
{
        struct inputCmd *currentCmd = head;
        int numPipes = pipeCount(head);
        int statusArrLen = 0;
        int statusArr[MAX_ARGS * numPipes];
        int status = 0;
        int pipeFds[numPipes * 2];
        pid_t pid;

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
                                        perror(" dup2"); /// cmdCounter-2 0 cmdCounter+1 1
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

                currentCmd = currentCmd->next;
                cmdCounter += 2;
        }
        for (int i = 0; i < 2 * numPipes; i++)
        {
                close(pipeFds[i]);
        }
        if (strpbrk(cmd, "&") != NULL)
        {
                waitpid(pid, &status, 0);
                fprintf(stderr, "+ completed '%s' ", cmd);

                for (int j = 0; j < statusArrLen; j++)
                {
                        fprintf(stderr, "[%d]", statusArr[j]);
                }
                fprintf(stderr, "\n");
        }
        else
        {
                for (int i = 0; i < numPipes + 1; i++)
                {
                        wait(&status);

                        if (i != 0 && status != 0)
                        {
                                statusArr[statusArrLen++] = WEXITSTATUS(status + 1);
                        }
                        else
                        {
                                statusArr[statusArrLen++] = WEXITSTATUS(status);
                        }
                }
                fprintf(stderr, "+ completed '%s' ", cmd);

                for (int j = 0; j < statusArrLen; j++)
                {
                        fprintf(stderr, "[%d]", statusArr[j]);
                }
                fprintf(stderr, "\n");
        }
        return 0;
}

int main(void)
{
        // implement cmd as struct for easier data manipulation
        char cmd[CMDLINE_MAX];
        putenv("/bin");

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
                printf("sshell$ ");
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
                if (!strcmp(cmd, "exit"))
                {
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
                        char buff[CMDLINE_MAX];
                        if (getcwd(buff, CMDLINE_MAX) == buff)
                        {
                                printf("%s\n", buff);
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
;
                        }
                        else
                        {
                                perror("getcwd");
                        }
                }
                else if (strcmp(storeCmd.args[0], "cd") == 0)
                {
                        int result = chdir(storeCmd.args[1]);
                        if (result == 0)
                        {
                                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
;
                        }
                        else
                        {
                                fprintf(stderr, "Error: cannot cd into directory\n");
                        }
                }
                else
                {
                        pipeExecute(&storeCmd, cmd);
                        freeLinked(&storeCmd);
                }
        }

        return EXIT_SUCCESS;
}
