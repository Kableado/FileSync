#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actionfilenode.h"
#include "actionfilenodecopy.h"
#include "actionfilenodesync.h"
#include "crc.h"
#include "filenode.h"
#include "fileutil.h"
#include "parameteroperation.h"
#include "util.h"

FileNode CheckDir(char *path, int recheck) {
	char dirNodesFile[MaxPath];
	FileNode fileNode;

	// Check directory
	snprintf(dirNodesFile, MaxPath, "%s/" FileNode_Filename, path);
	if (recheck) {
		Print("Checking Directory.. %s\n", path);
		long long tScan = Time_GetTime();
		fileNode = FileNode_Load(dirNodesFile);
		if (fileNode) {
			fileNode = FileNode_Refresh(fileNode, path);
		} else {
			fileNode = FileNode_Build(path);
		}
		tScan = Time_GetTime() - tScan;
		Print("\ttScan :");
		PrintElapsedTime(tScan);
		Print("\n");

		long long tSave = Time_GetTime();
		FileNode_Save(fileNode, dirNodesFile);
		tSave = Time_GetTime() - tSave;
		Print("\ttSave :");
		PrintElapsedTime(tSave);
		Print("\n");

	} else {
		Print("Loading Directory.. %s\n", path);
		fileNode = FileNode_Load(dirNodesFile);
		if (!fileNode) {
			Print("Error, no nodesFile.fs\n");
			return NULL;
		}
	}
	return fileNode;
}

void PrintStatistics(ActionFileNode actionFileNode,
					 ActionFileNodeResult result) {
	ActionQueueStatistics statistics;
	if (ActionFileNode_Statistics(actionFileNode, &statistics, result) == 0) {
		Print("Noting to do.\n");
		return;
	}
	Print("Statistics\n");

	Print("       % 8s    % 8s    % 8s\n", "Read", "Write", "Delete");
	Print("Left :");
	PrintDataSize(statistics.readLeft);
	PrintDataSize(statistics.writeLeft);
	PrintDataSize(statistics.deleteLeft);
	Print("\n");
	Print("Right:");
	PrintDataSize(statistics.readRight);
	PrintDataSize(statistics.writeRight);
	PrintDataSize(statistics.deleteRight);
	Print("\n");

	Print("\n");
	Print("Copy count     : % 10d\n", statistics.fullCopyCount);
	Print("Date copy count: % 10d\n", statistics.dateCopyCount);
	Print("Directory count: % 10d\n", statistics.directoryCount);
	Print("Delete count   : % 10d\n", statistics.deleteCount);
}

int Sync(char *pathLeft, char *pathRight, int reCheck, int dryRun) {
	FileNode fileNodeLeft;
	FileNode fileNodeRight;

	// Check and load directories
	if (!File_ExistsPath(pathLeft) || !File_IsDirectory(pathLeft)) {
		Print("Error, directory does not exist: %s\n", pathLeft);
		return 0;
	}
	if (!File_ExistsPath(pathRight) || !File_IsDirectory(pathRight)) {
		Print("Error, directory does not exist: %s\n", pathRight);
		return 0;
	}
	fileNodeLeft = CheckDir(pathLeft, reCheck);
	if (!fileNodeLeft) {
		return 0;
	}
	fileNodeRight = CheckDir(pathRight, reCheck);
	if (!fileNodeRight) {
		return 0;
	}

	// Build actions
	long long tBuild = Time_GetTime();
	Print("Building action list.. \n");
	ActionFileNode actionFileNode = NULL;
	actionFileNode = ActionFileNode_BuildSync(fileNodeLeft, fileNodeRight);
	tBuild = Time_GetTime() - tBuild;
	Print("\ttBuild:");
	PrintElapsedTime(tBuild);
	Print("\n");

	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode, ActionFileNodeResult_Nothing);
	} else {
		// Run action list
		if (ActionFileNode_RunList(actionFileNode, pathLeft, pathRight)) {
			PrintStatistics(actionFileNode, ActionFileNodeResult_Ok);
			CheckDir(pathLeft, reCheck);
			CheckDir(pathRight, reCheck);
		}
	}

	return (1);
}

int Copy(char *pathLeft, char *pathRight, int reCheck, int dryRun) {
	FileNode fileNodeLeft;
	FileNode fileNodeRight;

	// Check and load directories
	if (!File_ExistsPath(pathLeft) || !File_IsDirectory(pathLeft)) {
		Print("Error, directory does not exist: %s\n", pathLeft);
		return 0;
	}
	if (!File_ExistsPath(pathRight) || !File_IsDirectory(pathRight)) {
		Print("Error, directory does not exist: %s\n", pathRight);
		return 0;
	}
	fileNodeLeft = CheckDir(pathLeft, reCheck);
	if (!fileNodeLeft) {
		return 0;
	}
	fileNodeRight = CheckDir(pathRight, reCheck);
	if (!fileNodeRight) {
		return 0;
	}

	// Build actions
	long long tBuild = Time_GetTime();
	Print("Building action list.. \n");
	ActionFileNode actionFileNode = NULL;
	actionFileNode = ActionFileNode_BuildCopy(fileNodeLeft, fileNodeRight);
	tBuild = Time_GetTime() - tBuild;
	Print("\ttBuild:");
	PrintElapsedTime(tBuild);
	Print("\n");

	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode, ActionFileNodeResult_Nothing);
	} else {
		// Run action list
		if (ActionFileNode_RunList(actionFileNode, pathLeft, pathRight)) {
			PrintStatistics(actionFileNode, ActionFileNodeResult_Ok);
			CheckDir(pathLeft, reCheck);
			CheckDir(pathRight, reCheck);
		}
	}

	return (1);
}

typedef struct SApplicationConfiguration TApplicationConfiguration,
	*ApplicationConfiguration;
struct SApplicationConfiguration {
	char *Dirs[10];
	bool NoScan;
	bool Dummy;
	bool Sync;
	bool Copy;
	bool NoAction;
	char *Log;
};
TApplicationConfiguration defaultConfig = {{NULL}, false, false, false,
										   false,  false, NULL};

bool SetParam_Dir(int argc, char *argv[], void *data) {
	ApplicationConfiguration config = (ApplicationConfiguration)data;
	if (File_ExistsPath(argv[0]) == 0) {
		Print("Error: Path \"%s\" does not exist.\n", argv[0]);
		return false;
	}
	char **destDir = config->Dirs;
	while (destDir[0] != NULL) {
		destDir++;
	}
	destDir[0] = argv[0];
	destDir++;
	destDir = NULL;
	return true;
}

bool SetParam_NoCheck(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->NoScan = true;
	return true;
}

bool SetParam_Dummy(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->Dummy = true;
	return true;
}

bool SetParam_Sync(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->Sync = true;
	return true;
}

bool SetParam_Copy(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->Copy = true;
	return true;
}

bool SetParam_Log(int argc, char *argv[], void *data) {
	ApplicationConfiguration config = (ApplicationConfiguration)data;
	config->Log = argv[0];
	Print_SetOutFile(config->Log);
	return true;
}

bool Func_Scan(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->NoAction = true;

	// Scan directory information tree and save
	long long tScan = Time_GetTime();
	FileNode fileNode;
	Print("Building FileNode..\n");
	fileNode = FileNode_Build(argv[0]);
	tScan = Time_GetTime() - tScan;
	Print("\ttScan :");
	PrintElapsedTime(tScan);
	Print("\n");
	FileNode_Save(fileNode, argv[1]);
	return true;
}

bool Func_Rescan(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->NoAction = true;

	// Scan directory information and save tree
	FileNode fileNode;
	Print("Loading FileNode..\n");
	fileNode = FileNode_Load(argv[1]);
	if (fileNode) {
		Print("Rebuilding FileNode..\n");
		long long tScan = Time_GetTime();
		fileNode = FileNode_Refresh(fileNode, argv[0]);
		tScan = Time_GetTime() - tScan;
		Print("\ttScan :");
		PrintElapsedTime(tScan);
		Print("\n");
		FileNode_Save(fileNode, argv[1]);
	} else {
		Print("Building FileNode..\n");
		long long tScan = Time_GetTime();
		fileNode = FileNode_Build(argv[0]);
		tScan = Time_GetTime() - tScan;
		Print("\ttScan :");
		PrintElapsedTime(tScan);
		Print("\n");
		FileNode_Save(fileNode, argv[1]);
	}
	return true;
}

bool Func_Read(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->NoAction = true;

	// Read information tree from file
	FileNode fileNode;
	fileNode = FileNode_Load(argv[0]);
	if (fileNode) {
		FileNode_Print(fileNode);
	}
	return true;
}

bool Func_Check(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->NoAction = true;

	// Read directory information tree
	char *path = argv[0];
	FileNode fileNode;

	fileNode = CheckDir(path, 1);
	return true;
}

TParameterOperation _parameterOperations[] = {
	{"dir", 1, "Specify a directory", SetParam_Dir},
	{"nocheck", 0, "Do not check for changes on directories", SetParam_NoCheck},
	{"dummy", 0, "Do not perform operations", SetParam_Dummy},
	{"copy", 0, "Copy first directory to second directory", SetParam_Copy},
	{"sync", 0, "Synchronize between two directories", SetParam_Sync},
	{"log", 1, "Log actions to file", SetParam_Log},

	{"scan", 2, "Scan directory and save to filenode file", Func_Rescan},
	{"rescan", 2, "Rescan directory and save to filenode file", Func_Rescan},
	{"read", 1, "Read filenode file", Func_Read},
	{"check", 1, "Check changes on a directory", Func_Check},

	{NULL, 0, NULL, NULL},
};

int main(int argc, char *argv[]) {
	TApplicationConfiguration config = defaultConfig;

	int parameterParsingResult =
		ParameterOperation_Parse(argc, argv, _parameterOperations, &config);
	if (parameterParsingResult <= 0) {
		ParameterOperation_PrintHelp(_parameterOperations);
		return 0;
	}
	if (config.NoAction) {
		return 0;
	}

	Print("\n================================ FileSync "
		  "===================================\n");

	if (config.Copy == false && config.Sync == false) {
		Print("Error: Action not specified.\n");
		return 0;
	}
	if (config.Dirs[0] == NULL || config.Dirs[1] == NULL) {
		Print("Error: Two directories are needed.\n");
		return 0;
	}
	if (config.Copy) {
		Copy(config.Dirs[0], config.Dirs[1], (config.NoScan == false),
			 config.Dummy);
	}
	if (config.Sync) {
		Sync(config.Dirs[0], config.Dirs[1], (config.NoScan == false),
			 config.Dummy);
	}

	return 0;
}
