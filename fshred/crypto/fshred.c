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

#include "fshred.h"

/* file shredding */

/* shred a file's contents */
int fshred(FILE *fp, unsigned long lim, unsigned int rounds) {
  return fshred_using_path(fp, lim, rounds, URANDOM);
}

/* shred a file's contents using a path as input */
int fshred_using_path(FILE *fp, unsigned long lim, unsigned int rounds,
    const char *path) {
  if (fp != NULL && path != NULL) {
    int buffer_size, r, start;
    char buffer[buffer_size = (RANDOM_BUFLEN < lim ? RANDOM_BUFLEN : lim)];
    unsigned long i;
    fflush(fp);
    start = ftell(fp);
    
    for (r = 0; r < rounds; r++) {
      i = 0L;
      
      while (i < lim) {
        shred_using_path(buffer, buffer_size, 1, path);
        fwrite(buffer, buffer_size, 1, fp);
        fflush(fp);
        i += buffer_size;
      }
      shred_using_path(buffer, lim % buffer_size, 1, path);
      fwrite(buffer, lim % buffer_size, 1, fp);
      fflush(fp);
    }
    fseek(fp, start, SEEK_SET);
    return 1;
  }
  return 0;
}

/* shred a file's contents given a path */
int pshred(const char *path, unsigned int rounds) {
  return pshred_using_path(path, rounds, URANDOM);
}

/* shred a file's contents given a path using a path as input */
int pshred_using_path(const char *path, unsigned int rounds,
    const char *inpath) {
  FILE *fp;
  int size, success;
  success = 0;
  
  if ((fp = fopen(path, "r+b")) != NULL) {
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    success = fshred_using_path(fp, size, rounds, inpath);
    fclose(fp);
  }
  return success;
}
