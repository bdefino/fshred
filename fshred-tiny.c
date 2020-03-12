/*
Copyright (C) 2020 Bailey Defino
<https://bdefino.github.io>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* a tiny/robust recursive shredder */

#undef BUFLEN
#undef IPATH
#undef OPTIONS
#undef OPTSTRING
#undef USAGE

#define BUFLEN 4194304 /* 1 << 22 (4MiB) */
#define IPATH "/dev/urandom"
#define OPTIONS ("OPTIONS\n" \
	"\t-b INT\n" \
	"\t\tset the buffer length manually\n" \
	"\t\t(defaults to 4MiB)\n" \
	"\t-c INT\n" \
	"\t\tset the overwrite count to INT\n" \
	"\t-f INT\n" \
	"\t\tset the limit on `nftw`'s open file descriptors\n" \
	"\t\t(defaults to 1024)\n" \
	"\t-h\n" \
	"\t\tprint this text and exit\n" \
	"\t-i INT\n" \
	"\t\tseek to INT (ONCE!) before reading the input\n" \
	"\t-l\n" \
	"\t\tfollow symbolic links\n" \
	"\t-m\n" \
	"\t\tallow spanning of multiple file systems\n" \
	"\t-o INT\n" \
	"\t\tseek to INT (each TARGET, each round)\n" \
	"\t\tbefore writing the output\n" \
	"\t-r INT\n" \
	"\t\toverwrite PATH INT times\n" \
	"\t-s PATH\n" \
	"\t\tdraw input entropy from PATH\n" \
	"\t-u\n" \
	"\t\tshred names & unlink entries beneath TARGET\n" \
	"\t\tmust be specified twice to unlink special files\n" \
	"TARGET\n" \
	"\toutput path (directories are handled recursively)\n")
#define OPTSTRING "b:c:f:hi:lmo:r:s:u+"
#define USAGE ("a tiny/robust recursive shredder\n" \
	"Usage: %s [OPTIONS] TARGET...\n")

struct {
	size_t buflen;
	unsigned int fd_limit;
	int flags;
	int has_ocount;
	int ifd;
	off_t ioffset;
	char *ipath;
	off_t ocount;
	off_t ooffset;
	char *opath;
	unsigned int rounds;
	int unlink; /* count */
} MAIN = {
	.buflen = BUFLEN,
	.fd_limit = 1024,
	.flags = FTW_DEPTH /* enable directory unlinking */
		| FTW_MOUNT /* local filesystem */
		| FTW_PHYS, /* ignore symbolic links */
	.has_ocount = 0,
	.ifd = -1,
	.ioffset = 0,
	.ipath = IPATH,
	.ocount = 0,
	.ooffset = 0,
	.opath = NULL, /* MUST be explicitly specified */
	.rounds = 1,
	.unlink = 0
};

/* `memset` that won't be optimized out by the compiler */
static void *(* volatile memshred)(void *, int, size_t) = \
	(void *(* volatile)(void *, int, size_t)) &memset;

/* shred a file using the contents of another */
int
fshred(const int ofd, const size_t buflen, const int ifd, off_t lim)
{
	char buf[buflen];
	char *bufp;
	ssize_t cbuflen; /* current buffer length */
	int retval;
	ssize_t wbuflen; /* written buffer length */

	retval = 0;

	if (!buflen) {
		retval = -EINVAL;
		goto bubble;
	}

	if (ifd < 0) {
		retval = -EBADF;
		goto bubble;
	}

	if (ofd < 0) {
		retval = -EBADF;
		goto bubble;
	}

	while (lim > 0) {
		/* (partially) fill the buffer */

		cbuflen = read(ifd, buf, (buflen < lim ? buflen : lim));

		if (cbuflen <= 0) {
			retval = cbuflen ? -errno : -EIO;
			goto bubble_ofd_ok;
		}
		lim -= cbuflen;
		
		/* flush the buffer */

		for (bufp = buf; cbuflen > 0; ) {
			wbuflen = write(ofd, bufp, cbuflen);

			if (wbuflen < 0) {
				retval = -errno;
				goto bubble_ofd_ok;
			}
			bufp += wbuflen;
			cbuflen -= wbuflen;
		}
	}
bubble_ofd_ok:
	if (fdatasync(ofd)
			&& !retval) {
		retval = -errno;
	}
bubble:
	memshred(buf, '\0', buflen);
	return retval;
}

/* handle a callback from `ntfw` */
static int
fshred__nftw_callback(const char *opath, const struct stat *st, int info,
	struct FTW *walk)
{
	off_t ocount;
	int ofd;
	off_t opos;
	char *perrors;
	int retval;
	unsigned int round;

	ofd = -1;
	perrors = 0;
	retval = 0;

	if (!MAIN.buflen) {
		perrors = "buffer length is 0";
		retval = -EINVAL;
		goto bubble;
	}

	if (MAIN.ifd < 0) {
		perrors = (char *) MAIN.ipath;
		retval = -EBADF;
		goto bubble;
	}

	if (opath == NULL) {
		perrors = "no path provided";
		retval = -EFAULT;
		goto bubble;
	}

	if (st == NULL) {
		perrors = (char *) opath;
		retval = -EFAULT;
		goto bubble;
	}

	if (walk == NULL) {
		perrors = "no walk";
		retval = -EFAULT;
		goto bubble;
	}

	if (info == FTW_DP) {
		if (MAIN.unlink) {
			printf("Unlinking \"%s\"...\n", opath);
			
			if (rmdir(opath)) {
				perrors = (char *) opath;
				retval = -errno;
				goto bubble;
			}
		}
		goto bubble;
	}

	if (info != FTW_F
			&& info != FTW_SL) {
		goto bubble;
	}
	
	/* attempt to exclusively create the file */

	ofd = open(opath, O_CREAT | O_EXCL | O_TRUNC | O_WRONLY, S_IRWXU);

	if (ofd < 0) {
		if (errno == EEXIST) {
			/*
			the file exists,
			so attempt to open it as usual
			*/

			ofd = open(opath, O_RDONLY | O_WRONLY);
		}

		if (ofd < 0) {
			perrors = (char *) opath;
			goto bubble_ofd_ok;
		}
	}

	/* compute output count */

	ocount = MAIN.ocount;

	if (!MAIN.has_ocount) {
		ocount = st->st_size;

		if (S_ISBLK(st->st_mode)
				|| S_ISDIR(st->st_mode)) {
			ocount = lseek(ofd, 0, SEEK_END);

			if (ocount < 0) {
				perrors = (char *) opath;
				retval = -errno;
				goto bubble_ofd_ok;
			}
		}
	}

	/* shred */

	for (round = 0; round < MAIN.rounds; round++) {
		opos = lseek(ofd, MAIN.ooffset, SEEK_SET);

		if (opos < 0) {
			perrors = (char *) opath;
			retval = -errno;
			goto bubble_ofd_ok;
		}
		printf("Shredding \"%s\" (round %u/%u)...\n", opath, round + 1,
			MAIN.rounds);
		retval = fshred(ofd, MAIN.buflen, MAIN.ifd, ocount - opos);

		if (retval) {
			perrors = (char *) opath;
			goto bubble_ofd_ok;
		}
	}

	if (MAIN.unlink
			&& ((!S_ISBLK(st->st_mode)
					&& !S_ISCHR(st->st_mode))
				|| MAIN.unlink > 1)) {
		/* unlink */

		printf("Unlinking \"%s\"...\n", opath);

		if (unlink(opath)
				&& !retval) {
			perrors = (char *) opath;
			retval = -errno;
		}
	}
bubble_ofd_ok:
	if (close(ofd)
			&& !retval) {
		retval = -errno;
	}
bubble:
	if (retval < 0) {
		errno = -retval;
		perror(perrors);
	}
	return retval;
}

/* print the usage */
static void
usage(const char *executable);

/* print the usage and option information */
static void
help(const char *executable)
{
	usage(executable != NULL ? executable : "?");
	fprintf(stderr, OPTIONS);
}

int
main(int argc, char **argv)
{
	int oisdir;
	int opt;
	char *perrors;
	int retval;
	struct stat ost;
	struct FTW _walk;

	perrors = NULL;
	retval = EXIT_SUCCESS;

	while (1) {
		opt = getopt(argc, argv, OPTSTRING);

		if (opt == -1) {
			break;
		}

		switch (opt) {
			case 'b':
				MAIN.buflen = atoi(optarg);
				MAIN.buflen = MAIN.buflen > 0
					? MAIN.buflen : BUFLEN;
				break;
			case 'c':
				MAIN.has_ocount = ~0;
				MAIN.ocount = atoi(optarg);
				MAIN.ocount = MAIN.ocount > 0
					? MAIN.ocount : 0;
				break;
			case 'f':
				MAIN.fd_limit = atoi(optarg);
				MAIN.fd_limit = MAIN.fd_limit > 0
					? MAIN.fd_limit : -1;
				break;
			case 'h':
				help(argv[0]);
				goto bubble;
			case 'i':
				MAIN.ioffset = atoi(optarg);
				break;
			case 'l':
				MAIN.flags &= ~FTW_PHYS;
				break;
			case 'm':
				MAIN.flags &= ~FTW_MOUNT;
				break;
			case 'o':
				MAIN.ooffset = atoi(optarg);
				break;
			case 'r':
				MAIN.rounds = atoi(optarg);
				MAIN.rounds = MAIN.rounds >= 0
					? MAIN.rounds : 0;
				break;
			case 's':
				MAIN.ipath = optarg;
				break;
			case 'u':
				MAIN.unlink++;
				break;

			/* fall-through */

			case ':':
				fprintf(stderr, "`%s` expected an argument.\n",
					optarg);
			case '?':
				fprintf(stderr, "`%s` isn't an option.\n",
					optarg);
			default:
				help(argv[0]);
				retval = EXIT_FAILURE;
				goto bubble;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		retval = EXIT_FAILURE;
		goto bubble;
	}

	MAIN.ifd = open(MAIN.ipath, O_RDONLY);

	if (MAIN.ifd < 0) {
		perrors = MAIN.ipath;
		retval = -errno;
		goto bubble;
	}
	
	if (lseek(MAIN.ifd, MAIN.ioffset, SEEK_SET) < 0) {
		perrors = MAIN.ipath;
		retval = -errno;
		goto bubble_ifd_ok;
	}

	for (; optind < argc; optind++) {
		MAIN.opath = argv[optind];

		if (stat(MAIN.opath, &ost)) {
			if (errno != ENOENT) {
				perrors = MAIN.opath;
				retval = -errno;
				goto bubble_ifd_ok;
			}

			/* signal for a file to be created */

			oisdir = 0;
		} else {
			oisdir = S_ISDIR(ost.st_mode);
		}
		
		if (oisdir) {
			retval = nftw(MAIN.opath, &fshred__nftw_callback,
				MAIN.fd_limit, MAIN.flags);
		} else {
			retval = fshred__nftw_callback(MAIN.opath, &ost, FTW_F,
				&_walk);
		}

		if (retval) {
			perrors = MAIN.opath;
			goto bubble_ifd_ok;
		}
	}
bubble_ifd_ok:
	if (close(MAIN.ifd)
			&& !retval) {
		perrors = MAIN.ipath;
		retval = -errno;
	}
bubble:
	if (retval < 0) {
		errno = -retval;
		perror(perrors);
	}
	return retval;
}

static void
usage(const char *executable)
{
	fprintf(stderr, USAGE, executable);
}

