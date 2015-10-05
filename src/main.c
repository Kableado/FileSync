#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "filenodecmp.h"

void Help(char *exe) {
	char exeFilename[MaxPath];
	File_GetName(exe, exeFilename);
	printff("Usage:\n");
	printff("    %s info [file] {[file] {..}}\n", exeFilename);
	printff("    %s scan [dir] [tree] \n", exeFilename);
	printff("    %s rescan [dir] [tree] \n", exeFilename);
	printff("    %s read [file] [tree]\n", exeFilename);
	printff("    %s dir [dir]\n", exeFilename);
	printff("\n");
	printff("    %s sync [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("    %s resync [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("    %s synctest [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("    %s resynctest [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("\n");
	printff("    %s copy [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("    %s recopy [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("    %s copytest [dirIzquierda] [dirDerecha]\n", exeFilename);
	printff("    %s recopytest [dirIzquierda] [dirDerecha]\n", exeFilename);
}

FileNode *CheckDir(char *path, int recheck);
int Sync(char *pathLeft, char *pathRight, int recheck, int dryRun);
int Copy(char *pathLeft, char *pathRight, int reCheck, int dryRun);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		Help(argv[0]);
		return 0;
	}

	if (!strcmp(argv[1], "info") && argc >= 3) {
		// Informacion de ficheros
		int i;
		for (i = 2; i < argc; i++) {
			if (File_ExistsPath(argv[i])) {
				FileNode *fileNode = FileNode_Build(argv[i]);
				FileNode_GetCRC(fileNode, argv[i]);
				FileNode_PrintNode(fileNode);
			}
		}
	}
	else if (!strcmp(argv[1], "scan") && argc == 4) {
		// Scan directory information tree and save
		long long tScan = Time_GetTime();
		FileNode *fileNode;
		printff("Building FileNode..\n");
		fileNode = FileNode_Build(argv[2]);
		tScan = Time_GetTime() - tScan;
		printff("tScan: %9lldus\n", tScan);
		FileNode_Save(fileNode, argv[3]);
	}
	else if (!strcmp(argv[1], "rescan") && argc == 4) {
		// Scan directory information and save tree
		FileNode *fileNode;
		printff("Loading FileNode..\n");
		fileNode = FileNode_Load(argv[3]);
		if (fileNode) {
			printff("Rebuilding FileNode..\n");
			long long tScan = Time_GetTime();
			fileNode = FileNode_Refresh(fileNode, argv[2]);
			tScan = Time_GetTime() - tScan;
			printff("tScan: %9lldus\n", tScan);
			FileNode_Save(fileNode, argv[3]);
		}
		else {
			printff("Building FileNode..\n");
			long long tScan = Time_GetTime();
			fileNode = FileNode_Build(argv[2]);
			tScan = Time_GetTime() - tScan;
			printff("tScan: %9lldus\n", tScan);
			FileNode_Save(fileNode, argv[3]);
		}
	}
	else if (!strcmp(argv[1], "read") && argc == 3) {
		// Read information tree from file
		FileNode *fileNode;
		fileNode = FileNode_Load(argv[2]);
		if (fileNode)
			FileNode_Print(fileNode);
	}
	else if (!strcmp(argv[1], "dir") && argc == 3) {
		// Read directory information tree
		char *path = argv[2];
		FileNode *fileNode;

		fileNode = CheckDir(path, 1);
		if (fileNode) {
			FileNode_Print(fileNode);
		}
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

FileNode *CheckDir(char *path, int recheck) {
	char dirNodesFile[MaxPath];
	FileNode *fileNode;

	// Check directory
	snprintf(dirNodesFile, MaxPath, "%s/"FileNode_Filename, path);
	if (recheck) {
		printff("Checking Directory.. %s\n", path);
		long long tScan = Time_GetTime();
		fileNode = FileNode_Load(dirNodesFile);
		if (fileNode) {
			fileNode = FileNode_Refresh(fileNode, path);
		}
		else {
			fileNode = FileNode_Build(path);
		}
		tScan = Time_GetTime() - tScan;
		printff("tScan: %9lldus\n", tScan);
		FileNode_Save(fileNode, dirNodesFile);
	}
	else {
		printff("Loading Directory.. %s\n", path);
		fileNode = FileNode_Load(dirNodesFile);
		if (!fileNode) {
			printff("Error, no nodesFile.fs\n");
			return NULL;
		}
	}
	return fileNode;
}

void PrintStatistics(ActionFileNode *actionFileNode) {
	ActionQueueStatistics statistics;
	ActionFileNode_Statistics(actionFileNode, &statistics);
	printff("Statistics\n");
	printff("       % 12s % 12s % 12s\n", "Read", "Write", "Delete");
	printff("Left : % 12lld % 12lld % 12lld\n", statistics.readLeft,
		statistics.writeLeft, statistics.deleteLeft);
	printff("Right: % 12lld % 12lld % 12lld\n", statistics.readRight,
		statistics.writeRight, statistics.deleteRight);
	printff("\n");
	printff("Copy count     : % 10d\n", statistics.fullCopyCount);
	printff("Date copy count: % 10d\n", statistics.dateCopyCount);
	printff("Directory count: % 10d\n", statistics.directoryCount);
	printff("Delete count   : % 10d\n", statistics.deleteCount);
}

int Sync(char *pathLeft, char *pathRight, int recheck, int dryRun) {
	FileNode *fileNodeLeft, *fileNodeRight;

	// Check and load directories
	if (!File_ExistsPath(pathLeft) || !File_IsDirectory(pathLeft)) {
		printff("Error, directory does not exist: %s\n", pathLeft);
		return 0;
	}
	if (!File_ExistsPath(pathRight) || !File_IsDirectory(pathRight)) {
		printff("Error, directory does not exist: %s\n", pathRight);
		return 0;
	}
	fileNodeLeft = CheckDir(pathLeft, recheck);
	if (!fileNodeLeft) {
		return 0;
	}
	fileNodeRight = CheckDir(pathRight, recheck);
	if (!fileNodeRight) {
		return 0;
	}

	// Build actions
	long long tBuild = Time_GetTime();
	printff("Building action list.. \n");
	ActionFileNode *actionFileNode = NULL;
	actionFileNode = ActionFileNode_BuildSync(fileNodeLeft, fileNodeRight);
	tBuild = Time_GetTime() - tBuild;
	printff("tBuild: %9lldus\n", tBuild);

	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode);
	}
	else {
		// Run action list
		ActionFileNode_RunList(actionFileNode, pathLeft, pathRight);
	}

	return (1);
}

int Copy(char *pathLeft, char *pathRight, int reCheck, int dryRun) {
	FileNode *fileNodeLeft, *fileNodeRight;

	// Check and load directories
	if (!File_ExistsPath(pathLeft) || !File_IsDirectory(pathLeft)) {
		printff("Error, directory does not exist: %s\n", pathLeft);
		return 0;
	}
	if (!File_ExistsPath(pathRight) || !File_IsDirectory(pathRight)) {
		printff("Error, directory does not exist: %s\n", pathRight);
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
	printff("Building action list.. \n");
	ActionFileNode *actionFileNode = NULL;
	actionFileNode = ActionFileNode_BuildCopy(fileNodeLeft, fileNodeRight);
	tBuild = Time_GetTime() - tBuild;
	printff("tBuild: %9lldus\n", tBuild);

	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode);
	}
	else {
		// Run action list
		ActionFileNode_RunList(actionFileNode, pathLeft, pathRight);
	}

	return (1);
}
