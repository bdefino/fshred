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
  "\t\tseek to INT before reading the input\n" \
  "\t-l\n" \
  "\t\tfollow symbolic links\n" \
  "\t-m\n" \
  "\t\tallow spanning of multiple file systems\n" \
  "\t-o INT\n" \
  "\t\tseek to INT before writing the output\n" \
  "\t-s PATH\n" \
  "\t\tdraw input entropy from PATH\n" \
  "\t-u\n" \
  "\t\tunlink entries under PATH\n")
#define OPTSTRING "b:c:f:hi:lmo:s:u+"
#define USAGE ("a tiny/robust recursive shredder\n" \
  "Usage: %s [OPTIONS] PATH...\n")

struct {
  size_t buflen;
  int fd_limit;
  int flags;
  int has_ocount;
  int ifd;
  off_t ioffset;
  char *ipath;
  off_t ocount;
  off_t ooffset;
  char *opath;
  int unlink;
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
  .unlink = 0
};

/* `memset` that won't be optimized out by the compiler */
static void *(* volatile memshred)(void *, int, size_t) = \
  (void *(* volatile)(void *, int, size_t)) &memset;

/* shred a file using the contents of another */
int fshred(const int ofd, int ifd, const size_t buflen, off_t lim) {
  char buf[buflen];
  char *bufp;
  ssize_t cbuflen; /* current buffer length */
  int retval;
  ssize_t wbuflen; /* written buffer length */

  /* prep bubbling */

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

  while (lim) {
    /* (partially) fill the buffer */

    bufp = buf;
    cbuflen = read(ifd, buf, (buflen < lim ? buflen : lim));

    if (cbuflen < 0) {
      retval = -errno;
      goto bubble;
    } else if (!cbuflen) {
      /* EOF */

      retval = -EIO;
      goto bubble;
    }
    lim -= cbuflen;
    
    /* flush the buffer */

    while (cbuflen) {
      wbuflen = write(ofd, bufp, cbuflen);

      if (wbuflen < 0) {
        retval = -errno;
        goto bubble;
      }
      bufp += wbuflen;
      cbuflen -= wbuflen;
    }
  }

bubble:

  if (ofd >= 0) {
    if (fdatasync(ofd)) {
      retval = -errno;
    }
  }
  memshred(buf, '\0', buflen);
  return retval;
}

/* handle a callback from `ntfw` */
static int fshred__nftw_callback(const char *opath, const struct stat *st,
    int info, struct FTW *walk) {
  int _errno;
  int ofd;
  off_t opos;
  int retval;

  /* prep bubbling */

  ofd = -1;
  retval = 0;

  if (!MAIN.buflen) {
    retval = -EINVAL;
    goto bubble;
  }

  if (MAIN.ifd < 0) {
    retval = -EBADF;
    goto bubble;
  }

  if (opath == NULL) {
    retval = -EFAULT;
    goto bubble;
  }

  if (st == NULL) {
    retval = -EFAULT;
    goto bubble;
  }

  if (walk == NULL) {
    retval = -EFAULT;
    goto bubble;
  }

  if (info == FTW_DP) {
    if (MAIN.unlink) {
      printf("Unlinking \"%s\"...\n", opath);
      
      if (rmdir(opath)) {
        retval = -errno;
        goto bubble;
      }
    }
  } else if (info == FTW_F
      || info == FTW_SL) {
    ofd = open(opath, O_CREAT | O_RDONLY | O_WRONLY);

    if (ofd < 0) {
      goto bubble;
    }
    
    opos = lseek(ofd, MAIN.ooffset, SEEK_SET);

    if (opos < 0) {
      retval = -errno;
      goto bubble;
    }

    printf("Shredding \"%s\"...\n", opath);
    retval = fshred(ofd, MAIN.ifd, MAIN.buflen,
      (MAIN.has_ocount ? MAIN.ocount : st->st_size) - opos);
  }

bubble:

  if (ofd >= 0) {
    _errno = errno;
    close(ofd);
    errno = _errno;

    if (MAIN.unlink) {
      printf("Unlinking \"%s\"...\n", opath);

      if (unlink(opath)
          && !retval) {
        retval = -errno;
      }
    }
  }
  return retval;
}

/* print the usage */
static void usage(const char *executable);

/* print the usage and option information */
static void help(const char *executable) {
  usage(executable != NULL ? executable : "?");
  fprintf(stderr, OPTIONS);
}

int main(int argc, char **argv) {
  int _errno;
  int opt;
  int retval;
  struct stat ost;
  struct FTW _walk;

  while (1) {
    opt = getopt(argc, argv, OPTSTRING);

    if (opt == -1) {
      break;
    }

    switch (opt) {
      case 'b':
        MAIN.buflen = atoi(optarg);
        MAIN.buflen = MAIN.buflen > 0 ? MAIN.buflen : BUFLEN;
        break;
      case 'c':
        MAIN.has_ocount = ~0;
        MAIN.ocount = atoi(optarg);
        MAIN.ocount = MAIN.ocount > 0 ? MAIN.ocount : 0;
        break;
      case 'f':
        MAIN.fd_limit = atoi(optarg);
        MAIN.fd_limit = MAIN.fd_limit > 0 ? MAIN.fd_limit : -1;
        break;
      case 'h':
        help(argv[0]);
        return 0;
      case 'i':
        MAIN.ioffset = atoi(optarg); /* allow negative values */
        break;
      case 'l':
        MAIN.flags &= ~FTW_PHYS;
        break;
      case 'm':
        MAIN.flags &= ~FTW_MOUNT;
        break;
      case 'o':
        MAIN.ooffset = atoi(optarg); /* allow negative values */
        break;
      case 's':
        MAIN.ipath = optarg;
        break;
      case 'u':
        MAIN.unlink = ~0;
        break;

      /* fall-through */

      case ':':
        fprintf(stderr, "`%s` expected an argument.\n", optarg);
      case '?':
        fprintf(stderr, "`%s` isn't an option.\n", optarg);
      default:
        help(argv[0]);
        retval = 1;
        goto bubble;
    }
  }

  if (optind >= argc) {
    usage(argv[0]);
    retval = 1;
    goto bubble;
  }

  MAIN.ifd = open(MAIN.ipath, O_RDONLY);

  if (MAIN.ifd < 0) {
    retval = -errno;
    goto bubble;
  }
  
  if (lseek(MAIN.ifd, MAIN.ioffset, SEEK_SET) < 0) {
    retval = -errno;
    goto bubble;
  }

  for (; optind < argc; optind++) {
    MAIN.opath = argv[optind];

    if (stat(MAIN.opath, &ost)) {
      retval = -errno;
      goto bubble;
    }
    
    if (S_ISDIR(ost.st_mode)) {
      retval = nftw(MAIN.opath, &fshred__nftw_callback, MAIN.fd_limit,
        MAIN.flags);
    } else {
      retval = fshred__nftw_callback(MAIN.opath, &ost, FTW_F, &_walk);
    }

    if (retval) {
      break;
    }
  }

bubble:

  if (retval < 0) {
    _errno = errno;
    errno = -retval;
    perror(MAIN.ifd >= 0 ? MAIN.opath : MAIN.ipath);
    errno = _errno;
  }

  if (MAIN.ifd >= 0) {
    close(MAIN.ifd);
  }
  return retval;
}

static void usage(const char *executable) {
  fprintf(stderr, USAGE, executable);
}

