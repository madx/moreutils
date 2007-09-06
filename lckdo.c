/* lckdo.c: run a program with a lock held,
 * to prevent multiple processes running in parallel.
 * Use just like `nice' or `nohup'.
 * Written by Michael Tokarev <mjt@tls.msk.ru>
 * Public domain.
 */

#define _GNU_SOURCE
#define _BSD_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sysexits.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <signal.h>

/* compile with -DUSE_FLOCK to use flock() instead of fcntl() */

#if !defined(USE_FLOCK) && !defined(F_SETLKW)
# define USE_FLOCK
#endif

#ifndef __GNUC__
# ifndef __attribute__
#   define __attribute__(x)
# endif
#endif

static char *progname;
static void
__attribute__((format(printf,3,4)))
__attribute__((noreturn))
error(int errnum, int exitcode, const char *fmt, ...) {
	va_list ap;
	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
	if (errnum)
		fprintf(stderr, ": %s\n", strerror(errnum));
	else
		fputs("\n", stderr);
	exit(exitcode);
}

static const char *lckfile;
static int quiet;

static void sigalarm(int sig) {
	if (quiet)
		_exit(EX_TEMPFAIL);
	error(0, EX_TEMPFAIL,
		"lock file `%s' is already locked (timeout waiting)", lckfile);
}

int main(int argc, char **argv) {
	int fd;
	int c;
	int create = O_CREAT;
	int dofork = 1;
	int waittime = 0;
	int shared = 0;
	int test = 0;
	int fdn = -1;
#ifndef USE_FLOCK
	struct flock fl;
#endif

	if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		argv[0] = ++progname;

	if (argc == 1) {
		printf(
			"%s: execute a program with a lock set.\n"
			"Usage: %s [options] lockfile program [arguments]\n"
			"where options are:\n"
			" -w - if the lock is already held by another process,\n"
			"   wait for it to complete instead of failing immediately\n"
			" -W sec - the same as -w but wait not more than sec seconds\n"
			" -e - execute the program directly, no fork/wait\n"
			"   (keeps extra open file descriptor)\n"
			" -E nnn - set the fd# to keep open in -e case (implies -e)\n"
			" -n - do not create the lock file if it does not exist\n"
			" -q - produce no output if lock is already held\n"
			" -s - lock in shared (read) mode\n"
			" -x - lock in exclusive (write) mode (default)\n"
			" -t - test for lock existence"
#ifndef USE_FLOCK
			" (just prints pid if any with -q)\n"
#endif
			"   (implies -n)\n"
		, progname, progname);
		return 0;
	}

	while ((c = getopt(argc, argv, "+wW:neE:sxtq")) != EOF) {
		switch(c) {
		case 'w':
			if (!waittime)
				waittime = -1;
			break;
		case 'W':
			if ((waittime = atoi(optarg)) < 1)
				error(0, EX_USAGE, "invalid wait time `%s'", optarg);
			break;
		case 't':
			test = 1;
			/* fall thru */
		case 'n':
			create = 0;
			break;
		case 'E':
			if ((fdn = atoi(optarg)) < 0 || fdn == 2)
				error(0, EX_USAGE, "invalid fd# `%s'", optarg);
			/* fall thru */
		case 'e':
			dofork = 0;
			break;
		case 's':
			shared = 1;
			break;
		case 'x':
			shared = 0;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			return EX_USAGE;
		}
	}

	argc -= optind; argv += optind;
	if (!argc || (!test && argc < 2))
		error(0, EX_USAGE, "too few arguments given");

	lckfile = *argv++;

#ifdef USE_FLOCK
	create |= O_RDONLY;
#else
	if (!test)
		create |= shared ? O_RDONLY : O_WRONLY;
#endif
	fd = open(lckfile, create, 0666);
	if (fd < 0) {
		if (test && errno == ENOENT) {
			if (!quiet)
				printf("lockfile `%s' is not locked\n", lckfile);
			return 0;
		}
		error(errno, EX_CANTCREAT, "unable to open `%s'", lckfile);
	}

	if (!test && fdn >= 0) {
		/* dup it early to comply with stupid POSIX fcntl locking
		 * semantics */
		if (dup2(fd, fdn) < 0)
			error(errno, EX_OSERR, "dup2(%d,%d) failed", fd, fdn);
		close(fd);
		fd = fdn;
	}

	if (test)
		waittime = 0;
	else if (waittime > 0) {
		alarm(waittime);
		signal(SIGALRM, sigalarm);
	}
#ifdef USE_FLOCK
	c = flock(fd, (shared ? LOCK_SH : LOCK_EX) | (waittime ? 0 : LOCK_NB));
	if (test && c < 0 &&
		(errno == EWOULDBLOCK || errno == EAGAIN || errno == EACCES)) {
		if (!quiet)
			printf("lockfile `%s' is locked\n", lckfile);
		else
			printf("locked\n");
		return EX_TEMPFAIL;
	}
#else
	memset(&fl, 0, sizeof(fl));
	fl.l_type = shared ? F_RDLCK : F_WRLCK;
	c = fcntl(fd, test ? F_GETLK : waittime ? F_SETLKW : F_SETLK, &fl);
	if (test && c == 0) {
		if (fl.l_type == F_UNLCK) {
			if (!quiet)
				printf("lockfile `%s' is not locked\n", lckfile);
			return 0;
		}
		if (!quiet)
			printf("lockfile `%s' is locked by process %d\n", lckfile, fl.l_pid);
		else
			printf("%d\n", fl.l_pid);
		return EX_TEMPFAIL;
	}
#endif
	if (waittime > 0)
		alarm(0);
	if (c < 0) {
		if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EACCES)
			error(errno, EX_OSERR, "unable to lock `%s'", lckfile);
		else if (quiet)
			return EX_TEMPFAIL;
		else
			error(0, EX_TEMPFAIL, "lockfile `%s' is already locked", lckfile);
	}

	if (dofork) {
		pid_t pid;
		int flags = fcntl(fd, F_GETFD, 0);
		if (flags < 0)
			error(errno, EX_OSERR, "fcntl() failed");
		fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
		pid = fork();
		if (pid < 0)
			error(errno, EX_OSERR, "unable to fork");
		else if (pid) {
			if (wait(&c) < 0)
				error(errno, EX_OSERR, "wait() failed");
			if (WIFSIGNALED(c))
				error(0, EX_SOFTWARE, "%s: %s", *argv,
						strsignal(WTERMSIG(c)));
			return WEXITSTATUS(c);
		}
	}
	execvp(*argv, argv);
	error(errno, EX_OSERR, "unable to execute %s", *argv);
}
