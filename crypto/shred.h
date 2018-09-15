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
#include <string.h>

/* in-memory shredding */

#ifndef CRYPTO_SHRED_H
#define CRYPTO_SHRED_H

#undef URANDOM
#define URANDOM "/dev/urandom"

/* shred an array in memory */
void shred(char *mem, size_t size, unsigned int rounds);

/* shred an array in memory using a path as input */
void shred_using_path(char *mem, size_t size, unsigned int rounds,
  const char *path);

#endif
