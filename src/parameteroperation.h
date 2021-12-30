// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

#ifndef _PARAMETEROPERATION_
#define _PARAMETEROPERATION_

#include "util.h"

typedef struct SParameterOperation TParameterOperation, *ParameterOperation;
struct SParameterOperation {
	char *Name;
	int NumItems;
	char *Description;
	bool (*SetFunc)(int argumentCount, char *arguments[], void *data);
};

int ParameterOperation_Parse(int argumentCount, char *arguments[],
							 TParameterOperation parameterOperations[],
							 void *data);

void ParameterOperation_PrintHelp(TParameterOperation parameterOperations[]);

#endif
