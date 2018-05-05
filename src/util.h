#ifndef _UTIL_
#define _UTIL_

#include <stdarg.h>
#include <stdlib.h>

#ifndef bool
typedef int bool;
#endif
#define true 1
#define false 0

/////////////////////////////
// String_Copy
//
// Copies a string.
char *String_Copy(char *str);

/////////////////////////////
// String_CompareCaseInsensitive
//
// Compares a string case insensitive
int String_CompareCaseInsensitive(char *str0, char *str1);

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
// Print_SetOutFile
//
void Print_SetOutFile(char *fileOut);

/////////////////////////////
// Print
//
// Prints the formated text screen
int Print(char *fmt, ...);

/////////////////////////////
// SetError
// GetError
//
void SetError(char *msg);
char *GetError();

#endif
