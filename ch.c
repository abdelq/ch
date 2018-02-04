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

int cd(char *path)
{
    if (!path) {
        setenv("PWD",getenv("HOME"),1);
		return chdir(getenv("HOME"));
    }
    if (strcmp(path,"-") == 0){
        setenv("PWD",getenv("OLDPWD"),1);
        puts(getenv("PWD"));
        return chdir(getenv("OLDPWD"));
    }
    char realpath_buf[PATH_MAX];
    char *realpath_ptr = realpath(path,realpath_buf);
    if(access(realpath_ptr,F_OK) == 0){
		setenv("PWD",realpath_ptr,1);
		return chdir(path);
    }
	return -1;
}

// FIXME Needs thorough review
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
			// TODO echo miaw$miaw
			// TODO Review https://gitlab.com/prenux/super_duper_shell/blob/remi2/ch.c
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
	// XXX If I do something like env NULL cmd NULL it might cause issues
}

int main(void)
{
	char *line, *cmd[_POSIX_ARG_MAX];
	pid_t cpid, w;
	int wstatus;

	compile_regex();

	while ((line = readline("% "))) {
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
			char *pwd = getenv("PWD");

			if (cmd[2]) {
				fprintf(stderr,
					"twado: cd: too many arguments\n");
				continue;
			}

			if (cd(cmd[1]) == -1) {
				setenv("PWD",pwd,1);
				fprintf(stderr, "twado: cd: %s: %s\n",
					cmd[1], strerror(errno));
				continue;
			}

			if(setenv("OLDPWD",pwd,1) == -1){
				fprintf(stderr,"twado: cd: Error updating OLDPWD");
				continue;
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

		if ((cpid = fork()) == -1) {
			perror("twado");
			exit(EXIT_FAILURE);
		}

		if (cpid == 0) {	/* Child */
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
		} else {	/* Parent */
			do {
				w = waitpid(cpid, &wstatus,
					    WUNTRACED | WCONTINUED);
				if (w == -1) {
					perror("twado");
					exit(EXIT_FAILURE);
				}
			} while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
		}
	}
}
