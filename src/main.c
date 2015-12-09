#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "actionfilenode.h"
#include "actionfilenodesync.h"
#include "actionfilenodecopy.h"

void Help(char *exe) {
	char exeFilename[MaxPath];
	File_GetName(exe, exeFilename);
	Print("Usage:\n");
	Print("    %s info [file] {[file] {..}}\n", exeFilename);
	Print("    %s scan [dir] [tree] \n", exeFilename);
	Print("    %s rescan [dir] [tree] \n", exeFilename);
	Print("    %s read [file] [tree]\n", exeFilename);
	Print("    %s dir [dir]\n", exeFilename);
	Print("    %s check [dir]\n", exeFilename);
	Print("\n");
	Print("    %s sync [dirA] [dirB]\n", exeFilename);
	Print("    %s resync [dirA] [dirB]\n", exeFilename);
	Print("    %s synctest [dirA] [dirB]\n", exeFilename);
	Print("    %s resynctest [dirA] [dirB]\n", exeFilename);
	Print("\n");
	Print("    %s copy [dirA] [dirB]\n", exeFilename);
	Print("    %s recopy [dirA] [dirB]\n", exeFilename);
	Print("    %s copytest [dirA] [dirB]\n", exeFilename);
	Print("    %s recopytest [dirA] [dirB]\n", exeFilename);
}

FileNode CheckDir(char *path, int recheck);
int Sync(char *pathLeft, char *pathRight, int recheck, int dryRun);
int Copy(char *pathLeft, char *pathRight, int reCheck, int dryRun);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		Help(argv[0]);
		return 0;
	}

	Print("\n================================ FileSync ===================================\n");
	if (!strcmp(argv[1], "info") && argc >= 3) {
		// Informacion de ficheros
		int i;
		for (i = 2; i < argc; i++) {
			if (File_ExistsPath(argv[i])) {
				FileNode fileNode = FileNode_Build(argv[i]);
				FileNode_LoadCRC(fileNode, argv[i]);
				FileNode_PrintNode(fileNode);
			}
		}
	}
	else if (!strcmp(argv[1], "scan") && argc == 4) {
		// Scan directory information tree and save
		long long tScan = Time_GetTime();
		FileNode fileNode;
		Print("Building FileNode..\n");
		fileNode = FileNode_Build(argv[2]);
		tScan = Time_GetTime() - tScan;
		Print("\ttScan :"); PrintElapsedTime(tScan); Print("\n");
		FileNode_Save(fileNode, argv[3]);
	}
	else if (!strcmp(argv[1], "rescan") && argc == 4) {
		// Scan directory information and save tree
		FileNode fileNode;
		Print("Loading FileNode..\n");
		fileNode = FileNode_Load(argv[3]);
		if (fileNode) {
			Print("Rebuilding FileNode..\n");
			long long tScan = Time_GetTime();
			fileNode = FileNode_Refresh(fileNode, argv[2]);
			tScan = Time_GetTime() - tScan;
			Print("\ttScan :"); PrintElapsedTime(tScan); Print("\n");
			FileNode_Save(fileNode, argv[3]);
		}
		else {
			Print("Building FileNode..\n");
			long long tScan = Time_GetTime();
			fileNode = FileNode_Build(argv[2]);
			tScan = Time_GetTime() - tScan;
			Print("\ttScan :"); PrintElapsedTime(tScan); Print("\n");
			FileNode_Save(fileNode, argv[3]);
		}
	}
	else if (!strcmp(argv[1], "read") && argc >= 3) {
		// Read information tree from file
		FileNode fileNode;
		fileNode = FileNode_Load(argv[2]);
		if (fileNode)
			FileNode_Print(fileNode);
	}
	else if (!strcmp(argv[1], "dir") && argc == 3) {
		// Read directory information tree
		char *path = argv[2];
		FileNode fileNode;

		fileNode = CheckDir(path, 1);
		if (fileNode) {
			FileNode_Print(fileNode);
		}
	}
	else if (!strcmp(argv[1], "check") && argc == 3) {
		// Read directory information tree
		char *path = argv[2];
		FileNode fileNode;

		fileNode = CheckDir(path, 1);
	}
	else if (argc == 4) {
		char *cmd = argv[1];
		char *pathLeft = argv[2];
		char *pathRight = argv[3];
		if (!strcmp(cmd, "sync")) {
			Sync(pathLeft, pathRight, 1, 0);
		}
		else if (!strcmp(cmd, "resync")) {
			Sync(pathLeft, pathRight, 0, 0);
		}
		else if (!strcmp(cmd, "synctest")) {
			Sync(pathLeft, pathRight, 1, 1);
		}
		else if (!strcmp(cmd, "resynctest")) {
			Sync(pathLeft, pathRight, 0, 1);
		}
		else if (!strcmp(cmd, "copy")) {
			Copy(pathLeft, pathRight, 1, 0);
		}
		else if (!strcmp(cmd, "recopy")) {
			Copy(pathLeft, pathRight, 0, 0);
		}
		else if (!strcmp(cmd, "copytest")) {
			Copy(pathLeft, pathRight, 1, 1);
		}
		else if (!strcmp(cmd, "recopytest")) {
			Copy(pathLeft, pathRight, 0, 1);
		}
	}
	else {
		Help(argv[0]);
	}

	return (0);
}

FileNode CheckDir(char *path, int recheck) {
	char dirNodesFile[MaxPath];
	FileNode fileNode;

	// Check directory
	snprintf(dirNodesFile, MaxPath, "%s/"FileNode_Filename, path);
	if (recheck) {
		Print("Checking Directory.. %s\n", path);
		long long tScan = Time_GetTime();
		fileNode = FileNode_Load(dirNodesFile);
		if (fileNode) {
			fileNode = FileNode_Refresh(fileNode, path);
		}
		else {
			fileNode = FileNode_Build(path);
		}
		tScan = Time_GetTime() - tScan;
		Print("\ttScan :"); PrintElapsedTime(tScan); Print("\n");
		FileNode_Save(fileNode, dirNodesFile);
	}
	else {
		Print("Loading Directory.. %s\n", path);
		fileNode = FileNode_Load(dirNodesFile);
		if (!fileNode) {
			Print("Error, no nodesFile.fs\n");
			return NULL;
		}
	}
	return fileNode;
}

void PrintStatistics(ActionFileNode actionFileNode) {
	ActionQueueStatistics statistics;
	if (ActionFileNode_Statistics(actionFileNode, &statistics) == 0) {
		Print("Noting to do.\n");
		return;
	}
	Print("Statistics\n");

	Print("       % 8s    % 8s    % 8s\n",
		"Read", "Write", "Delete");
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
	Print("\ttBuild:"); PrintElapsedTime(tBuild); Print("\n");

	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode);
	}
	else {
		// Run action list
		if (ActionFileNode_RunList(actionFileNode, pathLeft, pathRight)) {
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
	Print("\ttBuild:"); PrintElapsedTime(tBuild); Print("\n");

	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode);
	}
	else {
		// Run action list
		if (ActionFileNode_RunList(actionFileNode, pathLeft, pathRight)) {
			CheckDir(pathLeft, reCheck);
			CheckDir(pathRight, reCheck);
		}
	}

	return (1);
}