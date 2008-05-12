/* 
 *
 * Copyright 2008 Javier Merino <cibervicho@gmail.com>
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#define streq(a, b) (!strcmp((a), (b)))

static void stdin_to_stream(char *buf, ssize_t r, FILE *outf) {
	while (r > 0) {
		if (fwrite(buf, r*sizeof(char), 1, outf) < 1) {
			fprintf(stderr, "Write error\n");
			exit(EXIT_FAILURE);
		}
		r = read(0, buf, BUFSIZ*sizeof(char));
	}
	if (r == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
	ssize_t r;
	int run_if_empty;
	char **argv_exec;
	int fds[2];
	int child_status;
	pid_t child_pid;
	char buf[BUFSIZ];
	FILE *outf;

	if ((argc < 2) || ((argc == 2) && streq(argv[1], "-n"))) {
		fprintf(stderr, "Usage: ifne [-n] command [args]\n");
		return EXIT_FAILURE;
	}

	if (streq(argv[1], "-n")) {
		run_if_empty = 1;
		argv_exec = &argv[2];
	} else {
		run_if_empty = 0;
		argv_exec = &argv[1];
	}

	r = read(0, buf, BUFSIZ*sizeof(char));

	if ((r == 0) && !run_if_empty)
		return EXIT_SUCCESS;
	else if (r == -1) {
		perror("read");
		return EXIT_FAILURE;
	}

	if (pipe(fds)) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	if (r && run_if_empty) {
		/* don't run the subcommand if we read something from stdin and -n was set */
		/* But write stdin to stdout so ifne -n can be piped without sucking the stream */
		stdin_to_stream(buf, r, stdout);
		return EXIT_SUCCESS;
	}

	child_pid = fork();
	if (!child_pid) {
		/* child process: rebind stdin and exec the subcommand */
		close(fds[1]);
		if (dup2(fds[0], 0)) {
			perror("dup2");
			return EXIT_FAILURE;
		}

		execvp(*argv_exec, argv_exec);
		perror(*argv_exec);
		close(fds[0]);
		return EXIT_FAILURE;
	} else if (child_pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	/* Parent: write stdin to fds[1] */
	close(fds[0]);
	outf = fdopen(fds[1], "w");
	if (! outf) {
		perror("fdopen");
		exit(1);
	}

	stdin_to_stream(buf, r, outf);
	fclose(outf);

	if (waitpid(child_pid, &child_status, 0) != child_pid) {
		perror("waitpid");
		return EXIT_FAILURE;
	}
	if (WIFEXITED(child_status)) {
		return (WEXITSTATUS(child_status));
	} else if (WIFSIGNALED(child_status)) {
		raise(WTERMSIG(child_status));
		return EXIT_FAILURE;
	}

	return EXIT_FAILURE;
}
