/*
 *  sponge.c - read in all available info from stdin, then output it to
 *  file named on the command line
 *
 *  Copyright ©  2006  Tollef Fog Heen
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
/* MAX() */
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>
/* SIZE_MAX */
#include <stdint.h> 
#include <signal.h>

#include "physmem.c"

#define MIN_SPONGE_SIZE     sizeof(8192)
#define BUFF_SIZE           8192
#define DEFAULT_TMP_NAME    "/tmp/sponge.XXXXXX"
char tmpname[] = DEFAULT_TMP_NAME;

void usage() {
	printf("sponge <file>: suck in all input from stdin and write it to <file>\n");
	exit(0);
}

/* all the signal stuff copied from gnu sort */

/* The set of signals that are caught.  */
static sigset_t caught_signals;

/* Critical section status.  */
struct cs_status
{
  int valid; // was bool
  sigset_t sigs;
};

/* Enter a critical section.  */
static struct cs_status
cs_enter (void)
{
  struct cs_status status;
  status.valid = (sigprocmask (SIG_BLOCK, &caught_signals, &status.sigs) == 0);
  return status;
}

/* Leave a critical section.  */
static void
cs_leave (struct cs_status status)
{
  if (status.valid)
    {
      /* Ignore failure when restoring the signal mask. */
      sigprocmask (SIG_SETMASK, &status.sigs, NULL);
    }
}


static void cleanup() {
    unlink(tmpname);
}

static void
onexit_cleanup (void)
{
    struct cs_status cs = cs_enter ();
    cleanup ();
    cs_leave (cs);
}

static void
sighandler (int sig)
{
  if (! SA_NOCLDSTOP)
    signal (sig, SIG_IGN);

  cleanup ();

  signal (sig, SIG_DFL);
  raise (sig);
}


/* taken from coreutils sort */
static size_t
default_sponge_size (void)
{
  /* Let MEM be available memory or 1/8 of total memory, whichever
     is greater.  */
  double avail = physmem_available ();
  double total = physmem_total ();
  double mem = MAX (avail, total / 8);
  struct rlimit rlimit;

  /* Let SIZE be MEM, but no more than the maximum object size or
     system resource limits.  Avoid the MIN macro here, as it is not
     quite right when only one argument is floating point.  Don't
     bother to check for values like RLIM_INFINITY since in practice
     they are not much less than SIZE_MAX.  */
  size_t size = SIZE_MAX;
  if (mem < size)
    size = mem;
  if (getrlimit (RLIMIT_DATA, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#ifdef RLIMIT_AS
  if (getrlimit (RLIMIT_AS, &rlimit) == 0 && rlimit.rlim_cur < size)
    size = rlimit.rlim_cur;
#endif

  /* Leave a large safety margin for the above limits, as failure can
     occur when they are exceeded.  */
  size /= 2;

#ifdef RLIMIT_RSS
  /* Leave a 1/16 margin for RSS to leave room for code, stack, etc.
     Exceeding RSS is not fatal, but can be quite slow.  */
  if (getrlimit (RLIMIT_RSS, &rlimit) == 0 && rlimit.rlim_cur / 16 * 15 < size)
    size = rlimit.rlim_cur / 16 * 15;
#endif

  /* Use no less than the minimum.  */
  return MAX (size, MIN_SPONGE_SIZE);
}

int main(int argc, char **argv) {
	char *buf, *bufstart, *outname = NULL;
	size_t bufsize = BUFF_SIZE;
	size_t bufused = 0;
	ssize_t i = 0;
    FILE *tmpfile = 0;
	if (argc > 2 || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
		usage();
	}
	bufstart = buf = malloc(bufsize);
	if (!buf) {
		perror("failed to allocate memory");
		exit(1);
	}

  {
    static int const sig[] =
      {
    /* The usual suspects.  */
    SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
#ifdef SIGPOLL
    SIGPOLL,
#endif
#ifdef SIGPROF
    SIGPROF,
#endif
#ifdef SIGVTALRM
    SIGVTALRM,
#endif
#ifdef SIGXCPU
    SIGXCPU,
#endif
#ifdef SIGXFSZ
    SIGXFSZ,
#endif
      };
    enum { nsigs = sizeof sig / sizeof sig[0] };

#if SA_NOCLDSTOP
    struct sigaction act;

    sigemptyset (&caught_signals);
    for (i = 0; i < nsigs; i++)
      {
    sigaction (sig[i], NULL, &act);
    if (act.sa_handler != SIG_IGN)
      sigaddset (&caught_signals, sig[i]);
      }

    act.sa_handler = sighandler;
    act.sa_mask = caught_signals;
    act.sa_flags = 0;

    for (i = 0; i < nsigs; i++)
      if (sigismember (&caught_signals, sig[i]))
    sigaction (sig[i], &act, NULL);
#else
    for (i = 0; i < nsigs; i++)
      if (signal (sig[i], SIG_IGN) != SIG_IGN)
    {
      signal (sig[i], sighandler);
      siginterrupt (sig[i], 1);
    }
#endif
  }


    size_t mem_available = default_sponge_size();
	while ((i = read(0, buf, bufsize - bufused)) > 0) {
		bufused = bufused+i;
		if (bufused == bufsize) {
            if(bufsize >= mem_available) {
                if(!tmpfile) {
                    umask(077);
                    struct cs_status cs = cs_enter ();
                    int tmpfd = mkstemp(tmpname);
                    atexit(onexit_cleanup); // if solaris on_exit(onexit_cleanup, 0);
                    cs_leave (cs);
                    if(tmpfd < 0) {
                        perror("mkstemp failed to open a temporary file");
                        exit(1);
                    }
                    tmpfile = fdopen(tmpfd, "w+");
                }
                if(fwrite(bufstart, bufsize, 1, tmpfile) < 1) {
                    perror("writing to tempory file failed"); 
                    fclose(tmpfile);
                    exit(1);
                }
                bufused = 0;
            } else {
			bufsize *= 2;
			bufstart = realloc(bufstart, bufsize);
			if (!bufstart) {
    				perror("failed to realloc memory");
				exit(1);
			}
		}
		}
		buf = bufstart + bufused;
	}
	if (i < 0) {
		perror("failed to read from stdin");
		exit(1);
	}
	if (argc == 2) {
        outname = argv[1];
	}
    if(tmpfile) {
        if(fwrite(bufstart, bufused, 1, tmpfile) < 1) {
                perror("write tmpfile");
                fclose(tmpfile);
                exit(1);
        }
        if(outname) {
            fclose(tmpfile);
            if(rename(tmpname, outname)) {
                perror("error renaming temporary file to output file");
			exit(1);
		}
	}
	else {
            if(fseek(tmpfile, 0, SEEK_SET)) {
                perror("could to seek to start of temporary file");
                fclose(tmpfile);
                exit(1);
	}
            while (fread( buf, BUFF_SIZE, 1, tmpfile) < 1) {
                if(fwrite(buf, BUFF_SIZE, 1, stdout) < 1) { 
                    perror("error writing out merged file");
		exit(1);
	}
            }
            fclose(tmpfile);
            unlink(tmpname);
        }
    }
    else {
        if(outname) {
            FILE *outfd = fopen(outname, "w");
            if(outfd < 0) {
                perror("error opening output file");
		exit(1);
	}
            if(fwrite(bufstart, bufused, 1, outfd) < 1) {
                    perror("error writing out merged file");
                    exit(1);
            }
        }
        else if(fwrite(bufstart, bufused, 1, stdout) < 1) {
            perror("error writing out merged file");
            exit(1);
        }
    }
	return 0;
}
