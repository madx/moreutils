
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

int main(int argc, char **argv) {
	ssize_t r;
	int fds[2];
	int child_status;
	pid_t child_pid;
	char buf[BUFSIZ];

	if (argc < 2) {
		/* Noop */
		return EXIT_SUCCESS;
	}

	r = read(0, buf, BUFSIZ*sizeof(char));

	if (r == 0)
		return EXIT_SUCCESS;
	else if (r == -1) {
		perror("read");
		return EXIT_FAILURE;
	}

	if (pipe(fds)) {
		perror("pipe");
		return EXIT_FAILURE;
	}

	child_pid = fork();
	if (!child_pid) {
		/* child process: rebind stdin and exec the subcommand */
		close(fds[1]);
		if (dup2(fds[0], 0)) {
			perror("dup2");
			return EXIT_FAILURE;
                }

		execvp(argv[1], &argv[1]);
		perror(argv[1]);
		close(fds[0]);
		return EXIT_FAILURE;
	} else if (child_pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	/* Parent: write in fds[1] our stdin */
	close(fds[0]);

	do {
		if (write(fds[1], buf, r*sizeof(char)) == -1) {
			fprintf(stderr, "Write error to %s\n", argv[1]);
			exit(EXIT_FAILURE);
		}
		r = read(0, buf, BUFSIZ*sizeof(char));
	} while (r > 0);
	if (r == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	
	close(fds[1]);
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
