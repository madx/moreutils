/*
 * isutf8.c - do the input files look like valid utf-8 byte streams?
 * 
 * Copyright (C) 2005  Lars Wirzenius
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#define VERSION "1.0"

/* 
 * I'm pretty sure there is a fancy trick to do this without a loop,
 * but I'm too tired to figure it out now. --liw
 */
static int high_ones(int c) {
	int n;

	for (n = 0; (c & 0x80) == 0x80; c <<= 1)
		++n;	
	return n;
}

static int is_utf8_byte_stream(FILE *file, char *filename, int quiet) {
	int c, n, remaining_bytes;
	unsigned long line, col;
	
	remaining_bytes = 0;
	line = 1;
	col = 1;
	while ((c = getc(file)) != EOF) {
		n = high_ones(c);
		if (remaining_bytes > 0) {
			if (n == 1) {
				--remaining_bytes;
				if (remaining_bytes == 0)
					++col;
			} else
				goto error;
		} else if (n == 0) {
			/* 7-bit character, skip, but adjust position */
			if (c == '\n') {
				++line;
				col = 1;
			} else
				++col;
		} else if (n == 1)
			goto error; /* wrong place for continuation byte */
		else
			remaining_bytes = n - 1; /* start of multi-byte sequence */
	}
	if (remaining_bytes > 0)
		goto error;
	return 1;
	
error:
	if (!quiet) {
		printf("%s: line %lu, col %lu: invalid UTF-8 code\n", 
		       filename, line, col);
	}
	return 0;
}

static void usage(const char *program_name) {
	printf("Usage: %s [-hq] [--help] [--quiet] [file ...]\n", program_name);
	printf("Check whether input files are valid UTF-8.\n");
	printf("This is version %s.\n", VERSION);
}

int main(int argc, char **argv) {
	int i, ok;
	FILE *file;

	int quiet;
	struct option options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "quiet", no_argument, &quiet, 1 },
	};
	int opt;
	
	quiet = 0;
	
	while ((opt = getopt_long(argc, argv, "hq", options, NULL)) != -1) {
		switch (opt) {
		case 0:
			break;
			
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
			
		case 'q':
			quiet = 1;
			break;

		case '?':
			exit(EXIT_FAILURE);

		default:
			abort();
		}
	}

	if (optind == argc)
		ok = is_utf8_byte_stream(stdin, "stdin", quiet);
	else {
		ok = 1;
		for (i = optind; i < argc; ++i) {
			file = fopen(argv[i], "r");
			if (file == NULL) {
				fprintf(stderr, "isutf8: %s: error %d: %s\n", 
				                argv[i], errno, strerror(errno));
				ok = 0;
			} else {
				ok = is_utf8_byte_stream(file, argv[i], quiet) && ok;
				(void) fclose(file);
			}
		}
	}
	
	if (ok)
		exit(0);
	exit(EXIT_FAILURE);
}
