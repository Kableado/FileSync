// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

#include "parameteroperation.h"
#include "util.h"

int ParameterOperation_Parse(int argumentCount, char *arguments[],
							 TParameterOperation parameterOperations[],
							 void *data) {
	int processedParams = 0;
	char **currentArguments = arguments + 1;
	for (int i = 1; i < argumentCount; i++) {
		char *currentArgument = currentArguments[0];
		currentArguments++;
		if (currentArgument[0] != '-') {
			Print("Error: Garbage found \"%s\" in position %d.\n", arguments[i],
				  i + 1);
			return -1;
		}
		while (currentArgument[0] == '-') {
			currentArgument++;
		}
		bool processed = false;
		int j = 0;
		while (parameterOperations[j].Name != NULL) {
			ParameterOperation parameterOperation = &parameterOperations[j];
			if (String_CompareCaseInsensitive(currentArgument,
											  parameterOperation->Name) == 0) {
				if ((i + parameterOperation->NumItems) >= argumentCount) {
					Print("Error: Parsing parameter \"-%s\" in position %d, "
						  "missig parameter data.\n",
						  parameterOperations[j].Name, i + 1);
					return -1;
				}
				bool result = parameterOperation->SetFunc(
					parameterOperation->NumItems, currentArguments, data);
				if (result == false) {
					Print("Error: Parsing parameter \"-%s\" in position %d.\n",
						  parameterOperations[j].Name, i + 1);
					return -1;
				}
				currentArguments += parameterOperation->NumItems;
				i += parameterOperation->NumItems;
				processedParams++;
				processed = true;
				break;
			}
			j++;
		}
		if (processed == false) {
			Print("Error: Unknow parameter \"%s\" in position %d.\n",
				  arguments[i], i + 1);
			return -1;
		}
	}
	return processedParams;
}

void ParameterOperation_PrintHelp(TParameterOperation parameterOperations[]) {
	int i = 0;
	Print("Parameters:\n");
	while (parameterOperations[i].Name != NULL) {
		Print("\t-%s", parameterOperations[i].Name);
		for (int j = 0; j < parameterOperations[i].NumItems; j++) {
			Print(" [Item]");
		}
		Print(": %s.\n", parameterOperations[i].Description);
		i++;
	}
}
