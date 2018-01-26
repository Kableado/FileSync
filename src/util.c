#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "util.h"

/////////////////////////////
// String_Copy
//
// Copies a string.
char *String_Copy(char *str) {
	char *strnew;
	size_t len;
	len = strlen(str);
	strnew = (char *)malloc(len + 1);
	if (strnew != NULL) {
		strcpy(strnew, str);
	}
	return (strnew);
}

/////////////////////////////
// String_CompareCaseInsensitive
//
// Compares a string case insensitive
int String_CompareCaseInsensitive(char *str0, char *str1) {
	for (int i = 0; ; i++) {
		char c0 = tolower(str0[i]);
		char c1 = tolower(str1[i]);
		if (c0 != c1) {
			return (c0 < c1) ? -1 : 1;
		}

		if (c0 == '\0') {
			return 0;
		}
	}
}

#if WIN32
#include <windows.h>
// WIN32
/////////////////////////////
// Time_GetTime
//
// Gets the time in usecs. WIN32 version.
long long Time_GetTime() {
	LARGE_INTEGER freq;
	LARGE_INTEGER tim;
	long long int microt;

	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&tim);
	microt = (tim.QuadPart * 1000000) / freq.QuadPart;
	return (microt);
}

/////////////////////////////
// Time_Pause
//
// Pauses the execution for t usecs. WIN32 version.
void Time_Pause(int pausa) {
	long long tend, t, diff;

	t = Time_GetTime();
	tend = t + pausa;
	do {
		diff = tend - t;
		if (diff > 1000) {
			Sleep((int)diff / 1000);
		} else {
			Sleep(0);
		}
		t = Time_GetTime();
	} while (tend >= t);
}
#else
/////////////////////////////
// Time_GetTime
//
// Gets the time in usecs. UNIX version.
long long Time_GetTime() {
	struct timeval t;
	long long usecs;
	gettimeofday(&t, NULL);
	usecs = (t.tv_sec * 1000000ll) + (t.tv_usec);
	return (usecs);
}

/////////////////////////////
// Time_Pause
//
// Pauses the execution for t usecs. UNIX version.
void Time_Pause(int pausa) {
	struct timeval tv;
	tv.tv_sec = (long long)pausa / 1000000;
	tv.tv_usec = (long long)pausa % 1000000;
	select(0, NULL, NULL, NULL, &tv);
}
#endif // if WIN32

/////////////////////////////
// Time_GetTime
//
// Gets the current time in POSIX.
long long Time_GetCurrentTime() {
	time_t t = time(0);
	return (long long)t;
}

/////////////////////////////
// PrintElapsedTime
//
// Prints the elapsed time (input in microseconds (us))
int PrintElapsedTime(long long time) {
	if (time < 1000) {
		return Print("%8lld us", time);
	}
	double msTime = (double)time / 1000;
	if (msTime < 1000) {
		return Print("% 8.3f ms", msTime);
	}
	double seconds = msTime / 1000;
	if (seconds < 60) {
		return Print("% 8.3f s", seconds);
	}
	int minutes = (int)seconds / 60;
	seconds = seconds - (minutes * 60);
	int hours = minutes / 60;
	minutes = minutes % 60;
	return Print("%02d:%02d:%06.3f", hours, minutes, seconds);
}

/////////////////////////////
// PrintDataSize
//
// Prints the data size (input in bytes)
int PrintDataSize(long long size) {
	if (size < 1024) {
		return Print("%8lld B  ", size);
	}
	double kibSize = (double)size / 1024;
	if (kibSize < 1024) {
		return Print("% 8.3f KiB", kibSize);
	}
	double mibSize = kibSize / 1024;
	if (mibSize < 1024) {
		return Print("% 8.3f MiB", mibSize);
	}
	double gibSize = mibSize / 1024;
	if (gibSize < 1024) {
		return Print("% 8.3f GiB", gibSize);
	}
	double tibSize = gibSize / 1024;
	return Print("% 8.3f TiB", tibSize);
}


FILE *outFile = NULL;

/////////////////////////////
// Print_SetOutFile
//
void Print_SetOutFile(char *fileOut) {
	if (fileOut == NULL) { return; }
	outFile = fopen(fileOut, "a");
}


#define Print_BuferSize 4096
/////////////////////////////
// Print
//
// Prints the formated text screen
int Print(char *fmt, ...) {
	va_list ap;
	char buffer[Print_BuferSize];
	int n;

	// Print
	va_start(ap, fmt);
	//n = vprintf(fmt, ap);
	n = vsnprintf(buffer, Print_BuferSize, fmt, ap);
	va_end(ap);

	// Output to stdout
	fputs(buffer, stdout);
	fflush(stdout);

	// Output to outFile
	if(outFile != NULL){
		fputs(buffer, outFile);
		fflush(outFile);
	}

	return (n);
}

/////////////////////////////
// SetError
// GetError
//
char _errorMessage[2048] = "";
char _errorMessageTemp[2048] = "";
void SetError(char *msg) { strcpy(_errorMessage, msg); }
char *GetError() {
	strcpy(_errorMessageTemp, _errorMessage);
	strcpy(_errorMessage, "");
	return _errorMessageTemp;
}
