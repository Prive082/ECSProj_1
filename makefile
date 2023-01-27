CC=gcc
CFLAGS= -g -Wall -Wextra -Werror

sshell: sshell.o 
	$(CC) -o sshell sshell.o

clean:
	$(RM) sshell