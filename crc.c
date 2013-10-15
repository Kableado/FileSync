#include <stdio.h>

unsigned long CRCTable[256];
int CRCTable_initialized = 0;

#define CRC32_POLYNOMIAL     0xEDB88320L

void CRCTable_Init() {
	int i;
	int j;
	unsigned long crc;

	if (CRCTable_initialized) {
		return;
	}
	CRCTable_initialized = 1;

	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
			else
				crc >>= 1;
		}
		CRCTable[i] = crc;
	}
}

unsigned long CRC_Buffer(unsigned char *buffer, int len, unsigned long crc) {
	unsigned char *p;
	unsigned long temp1;
	unsigned long temp2;

	// Calcular CRC del buffer
	p = (unsigned char*) buffer;
	while (len-- != 0) {
		temp1 = (crc >> 8) & 0x00FFFFFFL;
		temp2 = CRCTable[((int) crc ^ *p++) & 0xff];
		crc = temp1 ^ temp2;
	}

	return (crc);
}

unsigned long CRC_File(FILE *file) {
	unsigned long crc;
	int count;
	unsigned char buffer[512];
	unsigned char *p;
	unsigned long temp1;
	unsigned long temp2;

	CRCTable_Init();

	crc = 0xFFFFFFFFL;
	for (;;) {
		// Llenar el buffer
		count = fread(buffer, 1, 512, file);
		if (count == 0)
			break;

		// Calcular CRC del buffer
		crc = CRC_Buffer(buffer, count, crc);
	}
	return (crc ^= 0xFFFFFFFFL);
}
