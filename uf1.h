#ifndef UF1_H
#define UF1_H

/** Class for holding full census data files. */
// There's already a class here 'Uf1' which is for the geo data of a set.
/* Binary format:
6 bytes file tag (ascii, probably "uSF1\0\0")
2 bytes state abbreviation
4 byte int, numbersPerLine
data: 4 byte ints, numbersPerLine wide, lines long. 4*m*n bytes.
*/

class mmaped;

class Uf1Data {
public:
  Uf1Data();
  ~Uf1Data();

  // read in census text file and write out binary version.
  static int processText(const char* filename, const char* outname);
  static int processText(int fd, int out);

  //int writeBinary(const char* filename);
  //int writeBinary(int fd);

  int mapBinary(const char* filename);

  // return a data value
  int value(int row, int column);
  
  void* data;
  // was data from malloc() ?
  bool allocated;
  // how many numbers in a row?
  int numIntFields;
  // if data isn't from malloc, it's a pointer into mmaped file:
  mmaped* map;
};

#endif /* UF1_H */
