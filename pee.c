#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Licensed under the GPL
 * Copyright (c) Miek Gieben, 2006
 */

/* like tee(1), but then connect to other programs using
 * pipes _and_ output to standard output
 */

int
close_pipes(FILE **p, size_t i) 
{
	int ret=EXIT_SUCCESS;
	size_t j;
	for (j = 0; j < i; j++) {
		int r = pclose(p[j]);
		if (WIFEXITED(r))
			ret |= WEXITSTATUS(r);
		else
			ret |= 1;
	}
	return ret;
}

int
main(int argc, char **argv) {
	size_t i, r;
	FILE **pipes;
	char buf[BUFSIZ];

	pipes = malloc(((argc - 1) * sizeof *pipes));
	if (!pipes) 
		exit(EXIT_FAILURE);

	for (i = 1; i < argc; i++) {
		pipes[i - 1] = popen(argv[i], "w");
		if (!pipes[i - 1]) {
			fprintf(stderr, "Can not open pipe to '%s\'\n", argv[i]);
			close_pipes(pipes, i);

			exit(EXIT_FAILURE);
		}
	}
	argc--;
	
	while(!feof(stdin) && (!ferror(stdin))) {
		r = fread(buf, sizeof(char), BUFSIZ, stdin);
		for(i = 0; i < argc; i++) {
			if (fwrite(buf, sizeof(char), r, pipes[i]) != r) {
				fprintf(stderr, "Write error to `%s\'\n", argv[i + 1]);
				close_pipes(pipes, i);
				exit(EXIT_FAILURE);
			}
		}
	}
	exit(close_pipes(pipes, argc));
}
