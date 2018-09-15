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

#include "shred.h"

/* in-memory shredding */

/* shred an array in memory */
void shred(char *mem, size_t size, unsigned int rounds) {
  shred_using_path(mem, size, rounds, URANDOM);
}

/* shred an array in memory using a path as input */
void shred_using_path(char *mem, size_t size, unsigned int rounds,
    const char *path) {
  if (mem != NULL && path != NULL && rounds > 0 && size > 0) {
    int attempts, bytes_read, i, p, pathlen;
    FILE *fp;
    
    if ((fp = fopen(path, "rb")) != NULL) { /* overwrite with contents */
      for (; 0 < rounds; rounds--) {
        attempts = i = 0;
        
        while (i < size && attempts < 2) {
          if (!(bytes_read = fread(mem + i, 1, size - i, fp))) {
            fseek(fp, 0, SEEK_SET);
            attempts++;
          } else {
            attempts = 0;
            i += bytes_read;
          }
        }
      }
      fclose(fp);
    } else if ((pathlen = strlen(path)) > 0) { /* overwrite with path */
      for (p = 0; 0 < rounds; rounds--) {
        for (i = 0; i < size; i++, p = (p + 1) % pathlen)
          *(mem + i) = *(path + p);
      }
    } else { /* overwrite with nulls */
      for (; 0 < rounds; rounds--) {
        for (i = 0; i < size; *(mem + i++) = '\0')
          ;
      }
    }
  }
}