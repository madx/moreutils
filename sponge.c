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
	printf("sponge <file>: suck in all input from stdin and write it to <file>\n");
	exit(0);
}

int main(int argc, char **argv) {
	char *buf, *bufstart;
	size_t bufsize = 8192;
	size_t bufused = 0;
	ssize_t i = 0;
	FILE *outf;
	
	if (argc > 2 || (argc == 2 && strcmp(argv[1], "-h") == 0)) {
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
  
	if (argc == 2) {
		outf = fopen(argv[1], "w");
		if (! outf) {
			fprintf(stderr, "Can't open %s: %s\n", argv[1], strerror(errno));
			exit(1);
		}
	}
	else {
		outf = stdout;
	}

	if (fwrite(bufstart, bufused, 1, outf) < 1) {
		perror("fwrite");
		exit(1);
	}

	if (fclose(outf) != 0) {
		perror("fclose");
		exit(1);
	}

	return 0;
}
