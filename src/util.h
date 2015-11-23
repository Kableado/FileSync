#ifndef _UTIL_
#define _UTIL_

#include <stdlib.h>
#include <stdarg.h>

/////////////////////////////
// String_Copy
//
// Copies a string.
char *String_Copy(char *str);

/////////////////////////////
// Time_GetTime
//
// Gets the current time in usecs.
long long Time_GetTime();


/////////////////////////////
// Time_Pause
//
// Pauses the execution for t usecs.
void Time_Pause(int pausa);

/////////////////////////////
// Time_GetTime
//
// Gets the current time in POSIX.
long long Time_GetCurrentTime();

/////////////////////////////
// PrintElapsedTime
//
// Prints the elapsed time (input in microseconds (us))
int PrintElapsedTime(long long time);

/////////////////////////////
// PrintDataSize
//
// Prints the data size (input in bytes)
int PrintDataSize(long long size);

/////////////////////////////
// Print
//
// Prints the formated text screen
int Print(char *fmt, ...);

#endif
