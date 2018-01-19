/*
 * Auteurs : Abdelhakim Qbaich, Rémi Langevin
 * Date : 2018-01-18
 * Problèmes connus : S.O.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_ARGS 128		// XXX

// TODO Rename function
// TODO Manage quotation marks and escape sequences
// XXX What if we pass MAX_ARGS
void parse(char **line, char **args)
{
	char *arg;
	while (arg = strsep(line, " ")) {	// XXX " \t\n\r"
		if (*arg) {
			*args++ = arg;
		}
	}
	*args = NULL;
}

int main(void)
{
	char *line;
	char *args[MAX_ARGS];
	pid_t pid;
	int status;

	while (line = readline("$ ")) {
		if (!*line) {
			continue;
		}

		add_history(line);
		parse(&line, args);

		// TODO Test
		if ((pid = fork()) < 0) {
			perror("ch");
			return EXIT_FAILURE;
		}

		if (pid == 0) {
			if (*args) {
				execvp(*args, args);	// TODO Read doc about execvp
				perror("ch");	// TODO Check output w/ perror(*args)
				// return EXIT_FAILURE ?
			}
			return EXIT_SUCCESS;	// XXX
		} else {
			waitpid(pid, &status, WUNTRACED);	// TODO Read the doc
		}
	}
}
