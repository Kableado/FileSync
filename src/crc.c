#include <stdio.h>

unsigned long _crcTable[256];
int _crcTableInitialized = 0;

#define CRC32_POLYNOMIAL	0xEDB88320L

void CRCTable_Init() {
	int i;
	int j;
	unsigned long crc;

	if (_crcTableInitialized) {
		return;
	}
	_crcTableInitialized = 1;

	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
			else
				crc >>= 1;
		}
		_crcTable[i] = crc;
	}
}

unsigned long CRC_BufferInternal(unsigned char *buffer, int len,
	unsigned long crc)
{
	unsigned char *p;

	// Calculate CRC from buffer
	p = (unsigned char*)buffer;
	while (len-- != 0) {
		unsigned long termA = (crc >> 8) & 0x00FFFFFFL;
		unsigned long termB = _crcTable[((int)crc ^ *p++) & 0xff];
		crc = termA ^ termB;
	}

	return (crc);
}

unsigned long CRC_Buffer(unsigned char *buffer, int len, unsigned long crc) {
	CRCTable_Init();
	return (CRC_BufferInternal(buffer, len, crc));
}

unsigned long CRC_File(FILE *file) {
	unsigned long crc;
	unsigned char buffer[512];

	CRCTable_Init();

	crc = 0xFFFFFFFFL;
	for (;;) {
		// Fill buffer
		int count = fread(buffer, 1, 512, file);
		if (count == 0)
			break;

		crc = CRC_BufferInternal(buffer, count, crc);
	}
	return (crc ^= 0xFFFFFFFFL);
}
