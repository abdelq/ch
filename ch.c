/**
 * Auteurs : Abdelhakim Qbaich, Rémi Langevin
 * Date : 2018-02-10
 * Problèmes connus : S.O.
 */

#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>

regex_t envvar;			// FIXME Rename
regex_t ass_regex;		// FIXME Rename

char *get_regerror(int errcode, regex_t * preg)
{
	size_t len = regerror(errcode, preg, NULL, 0);
	char *buf = malloc(len);	// XXX Verify
	regerror(errcode, preg, buf, len);
	return buf;
}

void compile_regex()
{
	int errcode;

	// TODO Use REG_NOSUB if possible
	if ((errcode = regcomp(&envvar, "\\$\\w+", REG_EXTENDED))) {
		fprintf(stderr, "%s\n", get_regerror(errcode, &envvar));	// XXX Test
		exit(EXIT_FAILURE);
	}
	// TODO Verify that REG_EXTENDED is needed
	// TODO Use REG_NOSUB if possible
	if ((errcode = regcomp(&ass_regex, "^\\w+=", REG_EXTENDED))) {
		fprintf(stderr, "%s\n", get_regerror(errcode, &ass_regex));	// XXX Test
		exit(EXIT_FAILURE);
	}
}

int cd(char *path)
{
	if (!path) {
		return chdir(getenv("HOME"));
	}
	return chdir(path);
}

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
			// XXX echo $HOME- /home/julius-
			// TODO What if echo $ ?
			// TODO What if echo $HOME$HOME
			if (!regexec(&envvar, arg, 0, NULL, 0)) {	// TODO Regex maybe ?
				arg = getenv(arg + 1);
			}

			*args++ = arg;
		}
	}
	*args = NULL;
	// XXX strsep memory clean ?
	// XXX Possibly add env data directly to arg after the null pointer
	// XXX POSIX MAX ARG says Maximum length of argument to the exec functions including environment data.
}

int main(void)
{
	char *line;
	char *cmd[_POSIX_ARG_MAX];
	pid_t pid;
	int status;

	compile_regex();

	while ((line = readline("% "))) {
		if (!*line) {
			continue;
		}
		add_history(line);

		parse(cmd, &line, " ");
		if (!*cmd) {
			continue;
		}

		/* Built-in commands */
		if (strcmp(cmd[0], "exit") == 0) {
			exit(EXIT_SUCCESS);
		} else if (strcmp(cmd[0], "cd") == 0) {
			if (cmd[2]) {
				fprintf(stderr,
					"twado: cd: too many arguments\n");
				continue;
			}

			if (cd(cmd[1]) == -1) {
				fprintf(stderr, "twado: cd: %s: %s\n",
					cmd[1], strerror(errno));
			}
			continue;
		}
		// FIXME
		/* Environment variables */
		/*if (!regexec(&ass_regex, cmd[0], 0, NULL, 0)) {
		   char *assign[MAX_ARGS];
		   parse(assign, &cmd[0], "=");
		   setenv(assign[0], assign[1], 1);
		   continue;
		   } */

		if ((pid = fork()) == -1) {
			perror("twado");
			exit(EXIT_FAILURE);
		}

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
			// TODO Integrate content from waitpid(2) (do while?)
			waitpid(pid, &status, WUNTRACED);
			// TODO && || WEXITSTATUS(status)
		}
	}

	// XXX Free allocated memory? line, *_regex, etc.
}
