
/*
 *  parallel.c - run commands in parallel until you run out of commands
 *
 *  Copyright © 2008  Tollef Fog Heen <tfheen@err.no>
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

#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>

void usage()
{
	printf("parallel [OPTIONS] command -- arguments: for each argument,"
	       "run command with argument\n");
	exit(1);
}

void exec_child(char **command, char *argument)
{
	char **argv;
	int argc = 0;
	int i;

	while (command[argc] != 0) {
		argc++;
	}
	argc++;
	argv = calloc(sizeof(char*), argc+1);
	for (i = 0; i < argc; i++) {
		argv[i] = command[i];
	}
	argv[i-1] = argument;
	if (fork() == 0) {
		/* Child */
		execvp(argv[0], argv);
		exit(1);
	}
	return;
}

int wait_for_child(void)
{
	id_t id_ignored = 0;
	siginfo_t infop;
	waitid(P_ALL, id_ignored, &infop, WEXITED);
	if (infop.si_code == CLD_EXITED)
		return infop.si_status;
	return 1;
}

int main(int argc, char **argv)
{
	int maxjobs = -1;
	int curjobs = 0;
	int maxload = -1;
	int opt;
	char **command = calloc(sizeof(char*), argc);
	char **arguments;
	int argidx = 0;
	int cidx = 0;
	int returncode = 0;

	while ((opt = getopt(argc, argv, "+hj:l:")) != -1) {
		switch (opt) {
		case 'h':
			usage();
			break;
		case 'j':
			maxjobs = atoi(optarg);
			break;
		case 'l':
			maxload = atoi(optarg);
			break;
		default: /* ’?’ */
			usage();
			break;
		}
	}

	if (maxjobs < 0) {
		usage();
	}

	while (optind < argc) {
		if (strcmp(argv[optind], "--") == 0) {
			int i;
			size_t mem = (sizeof (char*)) * (argc - optind);

			arguments = malloc(mem);
			if (! arguments) {
				exit(1);
			}
			memset(arguments, 0, mem);
			optind++; /* Skip the -- separator, skip after
				   * malloc so we have a trailing
				   * null */

			for (i = 0; i < argc - optind; i++) {
				arguments[i] = strdup(argv[optind + i]);
			}
			optind += i;
		} else {
			command[cidx] = strdup(argv[optind]);
			cidx++;
		}
		optind++;
	}

	while (arguments[argidx] != 0) {
		if (maxjobs > 0 && curjobs < maxjobs) {
			exec_child(command, arguments[argidx]);
			argidx++;
			curjobs++;
		}

		if (maxjobs > 0 && curjobs == maxjobs) {
			returncode |= wait_for_child();
			curjobs--;
		}
	}
	while (curjobs > 0) {
		returncode |= wait_for_child();
		curjobs--;
	}

	return returncode;
}

