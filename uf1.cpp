#include "uf1.h"
#include "tiger/mmaped.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Uf1Data::Uf1Data()
  : data(NULL), allocated(true), numIntFields(0), map(NULL) {
}
Uf1Data::~Uf1Data() {
  if ( allocated ) {
    if ( data != NULL ) {
      free(data);
    }
  } else {
    if ( map != NULL ) {
      map->close();
      delete map;
    }
  }
}

#if 0
static const int BADPOS = 0x7fffffff;

static inline int mynextchar(char* haystack, int pos, off_t lim, char needle) {
  while ( pos < lim ) {
    if ( haystack[pos] == needle ) {
      return pos;
    }
    pos++;
  }
  return BADPOS;
}
#endif

static inline char* mynextchar(char* pos, char* last, char needle) {
  while ( pos <= last ) {
    if ( *pos == needle ) {
      return pos;
    }
    pos++;
  }
  return NULL;
}
// Count comma separated elements before first newline
static int countElements(char* base, off_t lim) {
  int count = 0;
  int state = 0;
  int pos = 0;
  while ( true ) {
    char c = base[pos];
    if ( (c == '\n') || (c == '\r') ) {
      return count;
    }
    if ( c == ',' ) {
      if ( state == 0 ) {
        // empty string previous field, count it I guess
        fprintf(stderr, "warning, empty field %d\n", count + 1);
        count++;
      } else {
        state = 0;
      }
    } else if ( state == 0 ) {
      count++;
      state = 1;
    }
    pos++;
  }
}

static inline void writeOrDieF(int fd, const void* buf, size_t len, const char* file, int line) {
  ssize_t err = write(fd, buf, len);
  if ((err < 0) || (((size_t)err) != len)) {
    fprintf(stderr, "%s:%d %s\n", file, line, strerror(errno));
    exit(1);
  }
}
#define writeOrDie(a,b,c) writeOrDieF(a,b,c,__FILE__,__LINE__)

static char nullchars[7] = "\0\0\0\0\0\0";

static int processTextMmapped(const char* filename, const char* outname, mmaped& mf) {
  int toret = 0;
  int out;
  out = open(outname, O_WRONLY|O_CREAT, 0666);
  if ( out < 0 ) {
    mf.close();
    perror(outname);
    return -1;
  }
  char* base = (char*)mf.data;
  char* last = base + (mf.sb.st_size - 1);
  char* pos = base;
  char* nextcomma = mynextchar(base, last, ',');
  char* nextnl = mynextchar(base, last, '\n');
  int state = 0;
  int thisLineElementIndex = 0;
  int lineCount = 0;
  int32_t numbersPerLine;
  int32_t* values;
  if ( nextnl == NULL ) {
    fprintf(stderr, "%s: no \\n found in entire file!\n", filename );
    toret = -1;
    goto finish;
  }
  if ( nextcomma == NULL ) {
    fprintf(stderr, "%s: no \',\' found in entire file!\n", filename );
    toret = -1;
    goto finish;
  }
  numbersPerLine = countElements(base, mf.sb.st_size) - 2;
  fprintf(stderr, "%s: %d elements per line\n", filename, numbersPerLine);
  assert(numbersPerLine > 0);
  assert(numbersPerLine < 10000);
  values = new int32_t[numbersPerLine];
  // first, count numbers in the first line (will later assert that all lines are equal)
  // Write out header data: filetype, state, number of elements
  {
    int tlen = nextcomma - pos;
    // write out file type tag
    writeOrDie(out, pos, tlen);
    if ( tlen < 6 ) {
      writeOrDie(out, nullchars, 6 - tlen);
    }
    pos = nextcomma + 1;
    nextcomma = mynextchar(pos, last, ',');
    assert((nextcomma - pos) == 2);
    writeOrDie(out, pos, 2);
    pos = nextcomma + 1;
    nextcomma = mynextchar(pos, last, ',');
    assert(nextcomma != NULL);
    state = 1;
    thisLineElementIndex = 0;
    writeOrDie(out, &numbersPerLine, 4);
  }
  while ( pos <= last ) {
    switch ( state ) {
      case 0: { // start of line
        // skip first two fields: filetype,state
        nextcomma = mynextchar(pos, last, ',');
        assert(nextcomma != NULL);
        pos = nextcomma + 1;
        nextcomma = mynextchar(pos, last, ',');
        assert(nextcomma != NULL);
        pos = nextcomma + 1;
        state = 1;
        thisLineElementIndex = 0;
      }
        break;
      case 1: { // data members
        char* endptr;
        values[thisLineElementIndex] = strtol(pos, &endptr, 10);
        assert(endptr != pos);
        thisLineElementIndex++;
        if ( (*endptr == '\n') || (*endptr == '\r') ) {
          assert(thisLineElementIndex == numbersPerLine);
          writeOrDie(out, values, 4*numbersPerLine);
          lineCount++;
          state = 0;
          pos = endptr + 1;
          while ( (pos <= last) && ((*pos == '\n') || (*pos == '\r')) ) {
            pos++;
          }
        } else {
          pos = endptr + 1;
        }
      }
        break;
      default:
        assert(false);
        break;
    }
  }
  fprintf(stderr, "%s: %d lines\n", filename, lineCount);
  finish:
  mf.close();
  close(out);
  return toret;
}

int Uf1Data::processText(const char* filename, const char* outname) {
  mmaped mf;
  int out;
  int err = mf.open(filename);
  if ( err != 0 ) {
    int fd = open(filename, O_RDONLY, 0);
    if ( fd < 0 ) {
      perror(filename);
      return -1;
    }
    out = open(outname, O_WRONLY|O_CREAT, 0666);
    if ( out < 0 ) {
      close(fd);
      perror(outname);
      return -1;
    }
    err = processText(fd, out);
    close(fd);
    close(out);
    return err;
  }
  return processTextMmapped(filename, outname, mf);
}
int Uf1Data::processText(int fd, int out) {
  return -1;
}

int Uf1Data::mapBinary(const char* filename) {
return -1;
}

// return a data value
int Uf1Data::value(int row, int column) {
return -1;
}
