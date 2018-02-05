/**
 * Auteurs : Abdelhakim Qbaich, Rémi Langevin
 * Date : 2018-02-10
 * Problèmes connus : S.O.
 */

#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
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
	char *buf = malloc(len);	// XXX Verify for error
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
	if ((errcode = regcomp(&ass_regex, "^\\w+=", REG_EXTENDED | REG_NOSUB))) {
		fprintf(stderr, "%s\n", get_regerror(errcode, &ass_regex));	// XXX Test
		exit(EXIT_FAILURE);
	}
}

bool env_assign(char **args)
{
	for (int i = 0; args[i] != NULL; i++) {
		if (regexec(&ass_regex, args[i], 0, NULL, 0) == REG_NOMATCH) {
			return false;
		}
	}
	return true;
}

// TODO Allow usage of cd -
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
			// TODO Manage ~
			if (arg[0] == '~') {
				if (strlen(arg) == 1) {
					arg = getenv("HOME");
				}
			}
			// TODO Multiple variables
			if (!regexec(&envvar, arg, 0, NULL, 0)) {
				if (!(arg = getenv(arg + 1))) {
					continue;
				}
			}

			*args++ = arg;
		}
	}
	*args = NULL;
}

// TODO exit or return ?
int execute(char **cmd)
{
	pid_t cpid, w;
	int wstatus;

	if ((cpid = fork()) == -1) {
		perror("twado");
		exit(EXIT_FAILURE);
	}

	if (cpid == 0) {	/* Child */
		if (execvp(*cmd, cmd) == -1) {
			if (errno == ENOENT) {
				fprintf(stderr,
					"twado: %s: command not found\n", *cmd);
			} else {
				perror("twado");
			}
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	} else {		/* Parent. See : man 2 waitpid */
		do {
			w = waitpid(cpid, &wstatus, WUNTRACED | WCONTINUED);
			if (w == -1) {
				perror("twado");
				exit(EXIT_FAILURE);
			}

			if (WIFEXITED(wstatus)) {
				return WEXITSTATUS(wstatus);
			}
		} while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
	}
	return -1;		// XXX
}

int main(void)
{
	char *line, *cmd[_POSIX_ARG_MAX];

	compile_regex();

 main:	while ((line = readline("% "))) {
		if (!*line) {
			free(line);
			continue;
		}
		add_history(line);

		parse(cmd, &line, " ");	// XXX free(line)
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

		/* Environment variables */
		if (env_assign(cmd)) {
			for (int i = 0; cmd[i] != NULL; i++) {
				if (putenv(cmd[i]) != 0) {
					perror("twado");
					break;	// XXX
				}
			}
			continue;
		}

		// FIXME Better code
		int j = 0;
		for (int i = 0; cmd[i] != NULL; i++) {
			if (strcmp(cmd[i], "||") == 0) {
				cmd[i] = NULL;
				if (execute(&cmd[j]) == 0) {
					goto main;
				}
				j = i + 1;
			} else if (strcmp(cmd[i], "&&") == 0) {
				cmd[i] = NULL;
				if (execute(&cmd[j]) != 0) {
					goto main;
				}
				j = i + 1;
			}
		}
		execute(&cmd[j]);
	}
}
