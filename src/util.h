#ifndef _UTIL_
#define _UTIL_

#include <stdlib.h>
#include <stdarg.h>

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

int printff(char *fmt, ...);

#endif
