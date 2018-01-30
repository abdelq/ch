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
#include <limits.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_ARGS 128

int cd(char *path)
{
	if (!path) {
		return chdir(getenv("HOME"));
	}
	return chdir(path);
}

// TODO Possibly use flex/bison
void parse(char **args, char **line)
{
	char *arg;
	while ((arg = strsep(line, " "))) {
		if (*arg) {
			if (arg[0] == '~') {
				if (strlen(arg) == 1) {
					arg = getenv("HOME");
				}
				// TODO no such user or named directory si username en plus ?
				// TODO ~/Workspace/ should be valid
				// TODO ~julius should be valid...
			}
			if (arg[0] == '$' && strlen(arg) > 1) {
				arg = getenv(arg + 1);
			}

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

	// XXX Multiple spaces crashes it
	// TODO Check code of rc
	while ((line = readline("% "))) {
		if (!*line) {
			continue;
		}

		add_history(line);
		parse(cmd, &line);

		// Builtin Commands
		if (strcmp(cmd[0], "exit") == 0) {
			exit(EXIT_SUCCESS);
		} else if (strcmp(cmd[0], "cd") == 0) {
			// Too many arguments
			if (cmd[2]) {
				fprintf(stderr,
					"ch: %s: too many arguments\n", *cmd);
				continue;
			}

			if (cd(cmd[1]) == -1) {
				perror("ch");	// TODO Bash-like error
			}
			continue;
		}
		// Environment variable
		// TODO Look for =, for every equal found, abort if no = after in other commands ? KEK=kek ls gives shit to command
		// TODO Use putenv w/ string, no need to bother with splitting

		if ((pid = fork()) == -1) {
			perror("ch");
			exit(EXIT_FAILURE);
		}
		// TODO Move to another function
		if (pid == 0) {	// Child
			if (*cmd) {
				// TODO Use execve and then use new array for env variables
				/*execve(argv[0], &argv[0], envp); */
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
			// TODO do while ?
			// TODO Integrate content from waitpid(2)
			waitpid(pid, &status, WUNTRACED);
		}

		free(line);
	}
}
