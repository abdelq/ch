/**
 * Auteurs : Abdelhakim Qbaich, Rémi Langevin
 * Date : 2018-02-11
 * Problèmes connus :
 *      - Fuite de mémoire dans la fonction expand
 *      - Retour d'un string vide plutôt que de sauter une variable d'env.
 *      inexistante (e.g. echo $HOME $COCOLAPIN $HOME)
 *      - On ne vérifit pas la présence de ; avant done
 *      - Pas de commandes built-in à l'intérieur d'un for (cd, exit)
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
	regmatch_t pmatch[1];

	for (int i = 0; args[i] != NULL; i++) {
		while (regexec(&envget, args[i], 1, pmatch, 0) == 0) {
			// Copy $ENV substring
			int len = pmatch->rm_eo - pmatch->rm_so;
			char substr[len];
			strncpy(substr, args[i] + pmatch->rm_so, len);
			substr[len] = '\0';

			char *env = getenv(substr + 1);
			if (env == NULL)
				env = "";

			char *arg = malloc(pmatch->rm_so + 1 + strlen(env) + 1 + strlen(args[i] + pmatch->rm_so + len) + 1);	// XXX
			strncpy(arg, args[i], (size_t) pmatch->rm_so);
			arg[pmatch->rm_so] = '\0';	// XXX
			strcat(arg, env);
			strcat(arg, args[i] + pmatch->rm_so + len);
			args[i] = arg;
		}
	}

	/*if (arg[0] == '~') {
	   if (strlen(arg) == 1) {
	   arg = getenv("HOME");
	   }
	   } */
}

void parse(char **args, char **line, char *sep)
{
	//char *ln = *line; // XXX
	char *arg;
	while ((arg = strsep(line, sep))) {
		if (*arg) {
			*args++ = arg;
			// TODO Do the same for && and ||
			// TODO This should be looping
			// FIXME Less ugly + maybe strsep
			if (strcmp(arg, ";") != 0) {
				char *kek = strstr(arg, ";");
				if (kek) {
					*kek = '\0';
					if (*(kek + 1) == '\0') {	// ui;
						*args++ = ";";
					} else if (kek == arg) {	// ;echo
						*(args - 1) = ";";
						*args++ = arg + 1;
					} else {	// wow;echo
						*args++ = ";";
						*args++ = kek + 1;
					}
				}
			}
		}
	}
	*args = NULL;
	//free(ln); // XXX
}

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

// TODO Clean up
void run(char **cmd)
{
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
	if (j != -1 && cmd[j]) {
		execute(&cmd[j]);
	}
}

void loop(char **cmd)
{
	for_loop floop = {
		.var = cmd[1],
		.values = &cmd[3]
	};

	if (strcmp(cmd[2], "in") != 0) {
		fprintf(stderr, "twado: Malformed for loop: missing 'in' statement\n");	// XXX
		return;
	}
	if (strcmp(cmd[3], ";") == 0) {
		fprintf(stderr, "twado: Malformed for loop: missing a range\n");	// XXX
		return;
	}

	/* Iterate over range of values */
	int i = 3;
	while (strcmp(cmd[i], ";") != 0) {
		i++;
	}
	cmd[i] = NULL;		// Replaces ;

	if (strcmp(cmd[++i], "do") != 0) {
		fprintf(stderr, "twado: Malformed for loop: missing 'do' statement\n");	// XXX
		return;
	}
	if (strcmp(cmd[++i], ";") == 0) {
		fprintf(stderr, "twado: Malformed for loop: missing body\n");	// XXX
		return;
	}

	floop.body = &cmd[i];

	while (cmd[i]) {
		i++;
	}
	if (strcmp(cmd[--i], "done") != 0) {
		fprintf(stderr, "twado: Malformed for loop: missing 'done'\n");	// XXX
		return;
	}
	cmd[i] = NULL;		// Replaces done
	if (strcmp(cmd[--i], ";") != 0) {
		fprintf(stderr, "twado: Malformed for loop: missing ';' before done\n");	// XXX
		return;
	}
	cmd[i] = NULL;		// Replaces ;

	expand(floop.values);
	for (int i = 0; floop.values[i]; i++) {
		setenv(floop.var, floop.values[i], 1);	// XXX

		char *cmd[_POSIX_ARG_MAX];	// XXX
		int j;
		for (j = 0; floop.body[j]; j++) {
			cmd[j] = strdup(floop.body[j]);
		}
		cmd[j] = NULL;

		if (strcmp(*cmd, "for") == 0) {
			loop(cmd);
			unsetenv(floop.var);	// XXX
			continue;
		}
		expand(cmd);
		/*for (int k = 0; cmd[k] != NULL; k++) {
		   puts(cmd[k]);
		   } */
		run(cmd);
		unsetenv(floop.var);	// XXX
	}

	//floop = {.var = NULL};
}

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
			loop(cmd);
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

		run(cmd);
	}
}
