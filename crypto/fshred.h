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
#include <stdio.h>
#include <stdlib.h>

#include "shred.h"

/* file shredding */

#ifndef CRYPTO_FSHRED_H
#define CRYPTO_FSHRED_H

enum FSHRED_FLAGS { /* to be used with the flags in dirwalk.h */
  FSHRED_NO_COLOR = 1024
};

#undef RANDOM_BUFLEN
#define RANDOM_BUFLEN 4096

/* shred a file's contents */
int fshred(FILE *fp, unsigned long lim, unsigned int rounds);

/* shred a file's contents using a path as input */
int fshred_using_path(FILE *fp, unsigned long lim, unsigned int rounds,
  const char *path);

/* shred a file's contents given a path */
int pshred(const char *path, unsigned int rounds);

/* shred a file's contents given a path using a path as input */
int pshred_using_path(const char *path, unsigned int rounds,
  const char *inpath);

#endif
