/*
Copyright (C) 2018 Bailey Defino
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
#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "ansiescape.h"
#include "crypto/fshred.h"
#include "path.h"
#include "specify.h"

/* fshred shredding program */

#undef help
#define help() puts("OPTIONS\n" \
  "\t-c, --nocolor\tdon't colorize output\n" \
  "\t-h, --help\tdisplay this text and exit\n" \
  "\t-i, --infile=PATH\tpath to new bytes\n" \
  "\t-l, --followlinks\tfollow all symbolic links\n" \
  "\t-s, --showspecial\tinclude all special devices\n" \
  "ROUNDS\n" \
  "\tthe number of rounds to shred each encountered resource\n" \
  "\tdefaults to 1\n" \
  "PATH\n" \
  "\tproperly-escaped path")

#undef usage
#define usage() puts("shred PATH ROUNDS times\n" \
  "Usage: fshred [OPTIONS] [ROUNDS] PATH")

struct {
  int flags;
  char *inpath;
  int nftw_flags;
  char *path;
  unsigned int rounds;
  int specify_flags;
} FSHRED_MAIN_ARGS;

const unsigned int FSHRED_MAIN_NOPENFD = 32;

int FSHRED_MAIN_EXIT_STATUS = 0;

/* handler for nftw */
int fshred_main_nftw_handler(const char *fpath, const struct stat *sb,
    int typeflag, struct FTW *ftwbuf) {
  int fgcolor[1];
  struct stat lsb;
  
  if (typeflag == FTW_SL) /* symbolic link */
    lstat(fpath, &lsb);
  
  if (!(typeflag & FTW_SLN) && specify(sb, (typeflag == FTW_SL ? &lsb : sb),
      FSHRED_MAIN_ARGS.specify_flags)) { /* specified */
    if (pshred_using_path(fpath, FSHRED_MAIN_ARGS.rounds,
        FSHRED_MAIN_ARGS.inpath)) {
      printf("shredded ");
    } else {
      fgcolor[0] = ANSI_FG_BRIGHT_RED;
      ansi_fprint(stdout, "FAILED TO SHRED ", 1, fgcolor);
    }
    
    if (FSHRED_MAIN_ARGS.flags & FSHRED_NO_COLOR) {
      puts(fpath);
    } else {
      ansi_fprint_path_st(stdout, fpath, &lsb);
      putchar('\n');
    }
  }
  return FSHRED_MAIN_EXIT_STATUS;
}

/* signal handler */
void fshred_main_signal_handler(int sig);

/* run fshred */
int main(int argc, char *argv[]) {
  unsigned int i, opt, signals[5];
  struct option longopts[6];
  
  FSHRED_MAIN_ARGS.inpath = (PATH_SEP == '\\' ? "" : URANDOM);
  FSHRED_MAIN_ARGS.nftw_flags = FTW_PHYS;
  FSHRED_MAIN_ARGS.specify_flags = 0;
  FSHRED_MAIN_ARGS.path = NULL;
  FSHRED_MAIN_ARGS.rounds = 1;
  
  memcpy(longopts,
    (struct option []) {{"followlinks", no_argument, NULL, 'l'},
      {"help", no_argument, NULL, 'h'},
      {"infile", required_argument, NULL, 'i'},
      {"nocolor", no_argument, NULL, 'c'},
      {"showspecial", no_argument, NULL, 's'},
      {0, 0, 0, 0}},
    sizeof(longopts));
  
  memcpy(signals, (int []) {SIGHUP, SIGINT, SIGTERM, SIGUSR1, SIGUSR2}, 5);
  
  while ((opt = getopt_long(argc, argv, "-chi:ls", longopts, NULL)) != -1) {
    switch (opt) {
      case '\1':
        if (atoi(optarg) && atoi(optarg) != FSHRED_MAIN_ARGS.rounds)
          FSHRED_MAIN_ARGS.rounds = atoi(optarg);
        else
          FSHRED_MAIN_ARGS.path = optarg;
        break;
      case 'c':
        FSHRED_MAIN_ARGS.flags |= FSHRED_NO_COLOR;
        break;
      case 'h':
        usage();
        help();
        return 0;
      case 'i':
        FSHRED_MAIN_ARGS.inpath = optarg;
        break;
      case 'l':
        FSHRED_MAIN_ARGS.nftw_flags &= ~FTW_PHYS;
        FSHRED_MAIN_ARGS.specify_flags |= SPECIFY_DIR_LINKS | SPECIFY_BLK_LINKS 
          | SPECIFY_CHR_LINKS
          | SPECIFY_FIFO_LINKS | SPECIFY_REG_LINKS;
        break;
      case 's':
        FSHRED_MAIN_ARGS.specify_flags |= SPECIFY_BLKS | SPECIFY_CHRS
          | SPECIFY_FIFOS;
        break;
      default:
        usage();
        return 1;
    }
  }
  
  if (FSHRED_MAIN_ARGS.path == NULL) { /* the path must be made explicit */
    usage();
    return 1;
  }
  
  for (i = 0; i < sizeof(signals); i++)
    signal(signals[i], &fshred_main_signal_handler);
  nftw(FSHRED_MAIN_ARGS.path, &fshred_main_nftw_handler, FSHRED_MAIN_NOPENFD,
    FSHRED_MAIN_ARGS.nftw_flags);
  return FSHRED_MAIN_EXIT_STATUS;
}

/* signal handler */
void fshred_main_signal_handler(int sig) {
  ansi_reset_stdio();
  FSHRED_MAIN_EXIT_STATUS = 1;
}
