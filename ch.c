/*
 * Auteurs : Abdelhakim Qbaich, Rémi Langevin
 * Date : 2018-02-10
 * Problèmes connus : S.O.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_ARGS 128

void parse(char **args, char **line)
{
	char *arg;
	while (arg = strsep(line, " ")) {
		if (*arg) {
			*args++ = arg;
		}
	}
	*args = NULL;
}

int main(void)
{
	char *line;
	char *cmd[MAX_ARGS];
	pid_t pid;
	int status;

	while (line = readline("$ ")) {
		if (!*line) {
			continue;
		}

		add_history(line);
		parse(cmd, &line);

		if ((pid = fork()) == -1) {
			perror("ch");
			exit(EXIT_FAILURE);
		}
		// TODO Move to another function
		if (pid == 0) {	// Child
			if (*cmd) {
				if (execvp(*cmd, cmd) == -1) {
					if (errno == ENOENT) {
						fprintf(stderr,
							"ch: %s: command not found\n",
							*cmd);
					} else {
						perror("ch");
					}
					exit(EXIT_FAILURE);
				}
			}
			exit(EXIT_SUCCESS);
		} else {	// Parent
			// TODO Integrate content from waitpid(2)
			waitpid(pid, &status, WUNTRACED);
		}

		free(line);
	}
}
