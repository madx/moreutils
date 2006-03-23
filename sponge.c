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
#include <fcntl.h>
#include <errno.h>
#include <string.h>

void usage() {
	printf("sponge <file>: suck in all input from stdin and write it to <file>");
	exit(0);
}

int main(int argc, char **argv) {
	char *buf, *bufstart;
	size_t bufsize = 8192;
	size_t bufused = 0;
	ssize_t i = 0;
	int outfd;
	
	if (argc != 2) {
		usage();
	}
  
	bufstart = buf = malloc(bufsize);
	if (!buf) {
		perror("malloc");
		exit(1);
	}

	while ((i = read(0, buf, bufsize - bufused)) > 0) {
		bufused = bufused+i;
		if (bufused == bufsize) {
			bufsize *= 2;
			bufstart = realloc(bufstart, bufsize);
			if (!bufstart) {
				perror("realloc");
				exit(1);
			}
		}
		buf = bufstart + bufused;
	}
	if (i == -1) {
		perror("read");
		exit(1);
	}
  
	outfd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (outfd == -1) {
		fprintf(stderr, "Can't open %s: %s\n", argv[1], strerror(errno));
		exit(1);
	}

	i = write(outfd, bufstart, bufused);
	if (i == -1) {
		perror("write");
		exit(1);
	}

	i = close(outfd);
	if (i == -1) {
		perror("close");
		exit(1);
	}

	return 0;
}
