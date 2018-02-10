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

regex_t envget;			// Regex for getting an env. variable (e.g. $FOO)
regex_t envset;			// Regex for setting an env. variable (e.g. FOO=bar)

typedef struct {
	char *var;
	char **values;
	char **body;
} for_loop;

char *get_regerror(int errcode, regex_t * preg)
{
	size_t len = regerror(errcode, preg, NULL, 0);
	char *buf = malloc(len);
	if (buf == NULL) {
		return strerror(errno);
	}
	regerror(errcode, preg, buf, len);
	return buf;
}

void compile_regex()
{
	int errcode;

	if ((errcode = regcomp(&envget, "\\$\\w+", REG_EXTENDED))) {
		fprintf(stderr, "%s\n", get_regerror(errcode, &envget));
		exit(EXIT_FAILURE);
	}
	if ((errcode = regcomp(&envset, "^\\w+=", REG_EXTENDED | REG_NOSUB))) {
		fprintf(stderr, "%s\n", get_regerror(errcode, &envset));
		exit(EXIT_FAILURE);
	}
}

int env_assign(char **args)
{
	for (int i = 0; args[i] != NULL; i++) {
		if (regexec(&envset, args[i], 0, NULL, 0) == REG_NOMATCH) {
			return 0;
		}
	}
	return 1;
}

int cd(char *path)
{
	if (!path) {
		return chdir(getenv("HOME"));
	}
	return chdir(path);
}

void expand(char **args)
{
	// TODO KEK=$HOME should technically work
	// FIXME echo Command: $MAN:$VERSION:$LS
	for (int i = 0; args[i] != NULL; i++) {
		// TODO Multiple variables
		if (regexec(&envget, args[i], 0, NULL, 0) == 0) {
			if (!(args[i] = getenv(args[i] + 1))) {
				args[i] = "";	// TODO Add to problèmes connus
			}
		}
	}
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
			*args++ = arg;
		}
	}
	*args = NULL;
}

// XXX exit or return
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
	return EXIT_FAILURE;	// XXX
}

/*void clear(char *cmd[])
{
	for (int i = 0; i < _POSIX_ARG_MAX; i++) {
		cmd[i] = NULL;
	}
}*/

/*int for_me(char **cmd)
{
	struct for_loop f;
	f.iter_var = cmd[1];
	if (strcmp(cmd[2], "in") != 0) {
		fprintf(stderr,
			"twaaadooo!!: Malformed for loop: missing 'in' statement");
		return EXIT_FAILURE;
	}
	//saves beginning of range
	f.iter_values = cmd + 3;
	char **body = f.body;
	// seek until end of range marked by ";"
	int i = 3;
	while (strcmp(cmd[i], ";") != 0)
		i++;
	if (i == 3) {
		fprintf(stderr,
			"twaaadooo!!: Malformed for loop: missing a range");
		return EXIT_FAILURE;
	}
	i++;
	if (strcmp(cmd[i], "do") != 0) {
		fprintf(stderr,
			"twaaadooo!!: Malformed for loop: missing 'do' statement");
		return EXIT_FAILURE;
	}
	// saves beginning of for body
	f.body = cmd + ++i;
	while (strcmp(cmd[i], "done") != 0) {
		i++;
		// ARG_MAX * A maximum of inner command
		if (i > _POSIX_ARG_MAX * 512) {
			fprintf(stderr,
				"twaaadooo!!: Malformed for loop: missing 'done' statement or body too long");
			return EXIT_FAILURE;
		}
	}
	// iterate over range
	for (int j = 0; strcmp(f.iter_values[j], ";"); j++) {
		setenv(f.iter_var, f.iter_values[j], 1);
		// iterate over cmds
		for (int k = 0; strcmp(f.body[k], "done") != 0; k++) {
			char *comm[_POSIX_ARG_MAX];
			int c = 0;
			for (k = k; strcmp(f.body[k], ";"); k++) {
				if (k > _POSIX_ARG_MAX - 2)
					break;
				comm[c++] = strdup(f.body[k]);
			}
			expand(comm);
			execute(comm);
			// ARE YOU FUNKIER DEAR ABDEL?
			clear(comm);
		}
	}
	return EXIT_SUCCESS;
}*/

int main(void)
{
	char *line, *cmd[_POSIX_ARG_MAX];

	compile_regex();

	while ((line = readline("% "))) {
		if (!(*line)) {
			free(line);
			continue;
		}
		add_history(line);

		parse(cmd, &line, " ");
		free(line);
		if (!(*cmd)) {
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

			expand(&cmd[1]);	// Expands env. variable
			if (cd(cmd[1]) == -1) {
				fprintf(stderr, "twado: cd: %s: %s\n",
					cmd[1], strerror(errno));
			}
			continue;
		} else if (strcmp(cmd[0], "for") == 0) {
			// FIXME For loop
			continue;
		}

		/* Environment variables */
		expand(cmd);
		if (env_assign(cmd)) {
			for (int i = 0; cmd[i] != NULL; i++) {
				if (putenv(cmd[i]) != 0) {
					perror("twado");
					//break; // XXX
				}
			}
			continue;
		}

		/* Execution */
		// XXX Clean up
		int j = 0;
		for (int i = 0; cmd[i] != NULL; i++) {
			if (strcmp(cmd[i], ";") == 0) {
				cmd[i] = NULL;
				execute(&cmd[j]);
				j = i + 1;
			} else if (strcmp(cmd[i], "||") == 0) {
				cmd[i] = NULL;
				if (execute(&cmd[j]) == 0) {
					j = -1;
					break;
				}
				j = i + 1;
			} else if (strcmp(cmd[i], "&&") == 0) {
				cmd[i] = NULL;
				if (execute(&cmd[j]) != 0) {
					j = -1;
					break;
				}
				j = i + 1;
			}
		}
		if (j != -1) {
			execute(&cmd[j]);
		}
	}
}
