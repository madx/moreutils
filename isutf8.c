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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>


#define VERSION "1.1"


/*
 * Code to indicate an invalid UTF8 character.
 */
enum { INVALID_CHAR = 0xffffffff };


/*
 * Produce shortest UTF8 encoding of a 31-bit value in 'u', returning it
 * in the array 'buf'. Return the number of bytes in the encoded value.
 * If the value is too large (more than 32 bits or would take more than
 * 'maxbytes' bytes), return -1.
 */
static int encodeutf8(unsigned long u, unsigned char *buf, size_t maxbytes)
{
        static const struct {
            int nbytes;
            unsigned long max;
        } tab[] = {
            { 1, 0x0000007F },
            { 2, 0x000007FF },
            { 3, 0x0000FFFF },
            { 4, 0x001FFFFF },
            { 5, 0x03FFFFFF },
            { 6, 0x7FFFFFFF },
        };
        static const int ntab = sizeof(tab) / sizeof(tab[0]);
        int i, j;

        if (u > tab[ntab-1].max)
                return -1;

        for (i = 0; i < ntab; ++i) {
                if (u <= tab[i].max)
                    break;
        }
        assert(i < ntab);

        if (tab[i].nbytes > maxbytes)
                return -1;
        
        if (tab[i].nbytes == 1) { /* Special case */
                buf[0] = u;
        } else {
                for (j = tab[i].nbytes-1; j > 0; --j) {
                        buf[j] = 0x80 | (u & 0x3f);
                        u >>= 6;
                }
        
                unsigned char mask = ~(0xFF >> tab[i].nbytes);
                buf[0] = mask | u;
        }

        return tab[i].nbytes;
}


/* 
 * Return number of ones at the top of a byte.
 *
 * I'm pretty sure there is a fancy trick to do this without a loop,
 * but I'm too tired to figure it out now. --liw
 */
static int high_ones(int c) {
        int n;

        for (n = 0; (c & 0x80) == 0x80; c <<= 1)
                ++n;    
        return n;
}


/*
 * Decode a UTF8 character from an array of bytes. Return character code.
 * Upon error, return INVALID_CHAR.
 */
static unsigned long decodeutf8(unsigned char *buf, int nbytes)
{
        unsigned long u;
        int i, j;
        
        if (nbytes <= 0)
                return INVALID_CHAR;
        
        if (nbytes == 1) {
                if (buf[0] >= 0x80)
                        return INVALID_CHAR;
                return buf[0];
        }
        
        i = high_ones(buf[0]);
        if (i != nbytes)
                return INVALID_CHAR;    
        u = buf[0] & (0xff >> i);
        for (j = 1; j < nbytes; ++j) {
                if ((buf[j] & 0xC0) != 0x80)
                            return INVALID_CHAR;
                u = (u << 6) | (buf[j] & 0x3f);
        }

        /* Conforming UTF-8 cannot contain codes 0xd800–0xdfff (UTF-16 
           surrogates) as well as 0xfffe and 0xffff. */
        if (u >= 0xD800 && u <= 0xDFFF)
            return INVALID_CHAR;
        if (u == 0xFFFE || u == 0xFFFF)
            return INVALID_CHAR;

        return u;
}


/*
 * Determine if the contents of an open file form a valid UTF8 byte stream.
 * Do this by collecting bytes for a character into a buffer and then
 * decode the bytes and re-encode them and compare that they are identical
 * to the original bytes. If any step fails, return 0 for error. If EOF
 * is reached, return 1 for OK.
 */
static int is_utf8_byte_stream(FILE *file, char *filename, int quiet) {
        enum { MAX_UTF8_BYTES = 6 };
        unsigned char buf[MAX_UTF8_BYTES];
        unsigned char buf2[MAX_UTF8_BYTES];
        int nbytes, nbytes2;
        int c;
        unsigned long code;
        unsigned long line, col, byteoff;

        nbytes = 0;
        line = 1;
        col = 1;
        byteoff = 0;
                
        for (;;) {
                c = getc(file);
    
                if (c == EOF || c < 0x80 || (c & 0xC0) != 0x80) {
                        /* New char starts, deal with previous one. */
                        if (nbytes > 0) {
                                code = decodeutf8(buf, nbytes);
                                if (code == INVALID_CHAR)
                                        goto error;
                                nbytes2 = encodeutf8(code, buf2, 
                                                     MAX_UTF8_BYTES);
                                if (nbytes != nbytes2 || 
                                    memcmp(buf, buf2, nbytes) != 0)
                                        goto error;
                                ++col;
                        }
                        nbytes = 0;
                        /* If it's UTF8, start collecting again. */
                        if (c != EOF && c >= 0x80)
                                buf[nbytes++] = c;
                } else {
                        /* This is a continuation byte, append to buffer. */
                        if (nbytes == MAX_UTF8_BYTES)
                                goto error;
                        buf[nbytes++] = c;
                }
    
                if (c == EOF)
                        break;
                else if (c == '\n') {
                        ++line;
                        byteoff = 0;
                        col = 1;
                } else
                        ++byteoff;
        }
        
        if (nbytes != 0)
                goto error;

	return 1;
	
error:
	if (!quiet) {
		printf("%s: line %lu, char %lu, byte offset %lu: "
		       "invalid UTF-8 code\n", filename, line, col, byteoff);
	}
	return 0;
}


static void usage(const char *program_name) {
	printf("Usage: %s [-hq] [--help] [--quiet] [file ...]\n", 
	       program_name);
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
				                argv[i], errno, 
				                strerror(errno));
				ok = 0;
			} else {
			        if (! is_utf8_byte_stream(file, argv[i], quiet))
			            ok = 0;
				(void) fclose(file);
			}
		}
	}
	
	if (ok)
		exit(0);
	exit(EXIT_FAILURE);
}
