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
#include <regex.h>

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_ARGS 128

regex_t var_regex;
regex_t home_regex;

int cd(char *path)
{
	if (!path) {
		return chdir(getenv("HOME"));
	}
	return chdir(path);
}

// TODO Possibly use flex/bison
void parse(char **args, char **line, char *sep)
{
	char *arg;
	while ((arg = strsep(line, sep))) {
		if (*arg) {
			if (arg[0] == '~') {
				if (strlen(arg) == 1) {
					arg = getenv("HOME");
				}
				// TODO no such user or named directory si username en plus ?
				// TODO ~/Workspace/ should be valid
				// TODO ~julius should be valid...
			}
			// TODO Regex maybe ?
		    if (!regexec(&var_regex, arg, 0, NULL, 0)){
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
	regex_t ass_regex;

	/* Match varname. Rules are:
	 * - Begin with alphanumeric char or '_'
	 * - No space around equal sign (Handled by string sep in parse func)
	 * - Avoid Special char like "?"  or "*"
	 */
	if (regcomp(&var_regex, "\\$[a-zA-Z0-9_]*", 0)){
		fprintf(stderr, "Could not compile regex\n");
		exit(EXIT_FAILURE);
	}
	if (regcomp(&ass_regex, "^[a-zA-Z_]*=", 0)){
		fprintf(stderr, "Could not compile regex\n");
		exit(EXIT_FAILURE);
	}
	// Regex for special case ~julius
	if (regcomp(&home_regex, "^~[^/].+", 0)){
		fprintf(stderr, "Could not compile regex\n");
		exit(EXIT_FAILURE);
	}

	while ((line = readline("abdshell% "))) {
		if (!*line) {
			continue;
		}

		add_history(line);

		parse(cmd, &line, " ");
		if (!*cmd) {
			continue;
		}
		// Builtin Commands
		if (strcmp(cmd[0], "exit") == 0) {
			exit(EXIT_SUCCESS);
		} else if (strcmp(cmd[0], "cd") == 0) {
			// Too many arguments
			if (cmd[2]) {
				fprintf(stderr, "twado: cd: too many arguments\n");	// XXX
				continue;
			}
			if (cd(cmd[1]) == -1) {
				perror("twado");	// TODO Bash-like error
			}
			continue;
		} else if (!regexec(&ass_regex, cmd[0], 0, NULL, 0)){
			char *assign[MAX_ARGS];
			parse(assign,&cmd[0], "=");
			setenv(assign[0], assign[1],1);
			continue;
		}

		if ((pid = fork()) == -1) {
			perror("twado");
			exit(EXIT_FAILURE);
		}
		// TODO Move to another function
		if (pid == 0) {	// Child
			// TODO Use execve and then use new array for env variables
			if (execvp(*cmd, cmd) == -1) {
				if (errno == ENOENT) {
					fprintf(stderr,
						"twado: %s: command not found\n",
						*cmd);
				} else {
					perror("twado");
				}
				exit(EXIT_FAILURE);
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
