#ifndef _CRC_
#define _CRC_

#include <stdio.h>

unsigned long CRC_Buffer(unsigned char *buffer, unsigned long len,
						 unsigned long crc);
unsigned long CRC_File(FILE *file);

#endif
