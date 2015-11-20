#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

char *String_Copy(char *str) {
	char *strnew;
	int len;
	len = strlen(str);
	strnew = malloc(len + 1);
	if (strnew != NULL) {
		strcpy(strnew, str);
	}
	return (strnew);
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
	return(microt);
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
		}
		else {
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
	return(usecs);
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

int printff(char *fmt, ...) {
	va_list ap;
	int n;

	// Print
	va_start(ap, fmt);
	n = vprintf(fmt, ap);
	va_end(ap);

	// Flush
	fflush(stdout);
	return(n);
}
