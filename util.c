#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

char *String_Copy(char *str){
	char *strnew;
	int len;
	len=strlen(str);
	strnew=malloc(len+1);
	strcpy(strnew,str);
	return(strnew);
}

