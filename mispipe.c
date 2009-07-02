/*
 * mispipe: written by Nathanael Nerode.
 *
 * Copyright 2004 Nathanael Nerode.
 *
 * Licensed under the GPL version 2 or above, and dual-licensed under the
 * following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 */

/*
 * Usage: mispipe <command1> <command2>
 * Run <command1> | <command2>, but return with the exit status of <command1>.
 *
 * This is designed for a very specific purpose: logging.
 * "foo | logger -t foo"
 * will return with the exit status of logger, not that of foo.
 * "mispipe foo 'logger -t foo'"
 * will return with the exit status of foo.
 */

/*
 * To do:
 * Make this into a fancy, internationalized, option-handling hellbeast.
 * (But why bother?  It does its job quite well.)
 */

#include <errno.h> /* errno */
#include <sys/types.h>
#include <unistd.h> /* pipe(), fork(),... */
#include <stdlib.h> /* system() */
#include <sys/wait.h> /* waitpid(), etc. */
#include <stdio.h>
#include <stdarg.h> /* va_list, for error() */

static const char* progname; /* Stores argv[0] */
static int filedes[2]; /* Stores pipe file descriptors */

/* Subroutine for 'warning' and 'error' which prefixes progname */
static void warning_prefix(void) {
	fputs(progname, stderr);
	fputs(": ", stderr);
}

/* Issue a warning, then die */
__attribute__(( noreturn, format (printf, 1, 2) ))
static void error(const char* formatstr, ...) {
	va_list ap;
	va_start(ap, formatstr);
	warning_prefix();
	vfprintf(stderr, formatstr, ap);
	va_end(ap);
	exit(1);
}

/* Issue a warning, then die, with errno */
__attribute__(( noreturn ))
static void error_with_errno(const char* message) {
	int saved_errno;
	saved_errno=errno;
	warning_prefix();
	fputs(message, stderr);
	fputs(": error number ", stderr);
	fprintf(stderr, "%i\n", saved_errno);
	exit(1);
}

/* Convert 'wait'/'system'-style exit status to 'exit'-style exit status */
__attribute__(( const ))
static int shorten_status(int status_big) {
	if (WIFEXITED(status_big))
		return WEXITSTATUS(status_big);
	if (WIFSIGNALED(status_big))
		return 128+WTERMSIG(status_big);
	if (WIFSTOPPED(status_big))
		return 128+WSTOPSIG(status_big);
#ifdef WCOREDUMP
	if (WCOREDUMP(status_big))
		 error("Ow, somebody dumped core!");
#endif
	error("shorten_status got an invalid status (?!)");
}

/* All the work for command 2. */
__attribute__(( noreturn ))
static void subprocess2(const char* cmd) {
	/* Close the old standard input. */
	if (close(0))
		error_with_errno("Failed (in child) closing standard input");
	/* Make the reading end of the pipe the new standard input. */
	if (dup2(filedes[0], 0) == -1)
		error_with_errno("Failed (in child) redefining standard input");
	/* Close the original file descriptor for it */
	if (close(filedes[0]))
		error_with_errno("Failed (in child) closing filedes[0]");
	/* Close the other end of the pipe. */
	if (close(filedes[1]))
		error_with_errno("Failed (in child) closing filedes[1]");
	/* Do the second command, and throw away the exit status. */
	system(cmd);
	/* Close the standard input. */
	if (close(0))
		error_with_errno("Failed (in child) closing standard output "
			" (while cleaning up)");
	exit(0);
}

int main (int argc, const char ** argv) {
	int status_big; /* Exit status, 'wait' and 'system' style */
	pid_t child2_pid;
	pid_t dead_pid;

	/* Set progname */
	progname = argv[0];

	/* Verify arguments */
	if (argc != 3) 
		error("Wrong number of args, aborting\n");
	/* Open a new pipe */
	if (pipe(filedes))
		error_with_errno("pipe() failed");

	/* Fork to run second command */
	child2_pid = fork();
	if (child2_pid == 0)
		subprocess2(argv[2]);
	if (child2_pid == -1)
		error_with_errno("fork() failed");

	/* Run first command inline (seriously!) */
	/* Close standard output. */
	if (close(1))
		error_with_errno("Failed closing standard output");
	/* Make the writing end of the pipe the new standard output. */
	if (dup2(filedes[1], 1) == -1)
		error_with_errno("Failed redefining standard output");
	/* Close the original file descriptor for it. */
	if (close(filedes[1]))
		error_with_errno("Failed closing filedes[1]");
	/* Close the other end of the pipe. */
	if (close(filedes[0]))
		error_with_errno("Failed closing filedes[0]");
	/* Do the first command, and (crucially) get the status. */
	status_big = system(argv[1]);

	/* Close standard output. */
	if (close(1))
		error_with_errno("Failed closing standard output (while cleaning up)");

	/* Wait for the other process to exit. */
	/* We don't care about the status. */
	dead_pid = waitpid(child2_pid, NULL, WUNTRACED);
	if (dead_pid == -1) {
		error_with_errno("waitpid() failed");
	}
	else if (dead_pid != child2_pid) {
		error("waitpid(): Who died? %i\n", dead_pid);
	}

	/* Return the desired exit status. */
	return shorten_status(status_big);
}
