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
	printf("Usage:\n");
	printf("    %s info [file] {[file] {..}}\n", exeFilename);
	printf("    %s scan [dir] [tree] \n", exeFilename);
	printf("    %s rescan [dir] [tree] \n", exeFilename);
	printf("    %s read [file] [tree]\n", exeFilename);
	printf("    %s dir [dir]\n", exeFilename);
	printf("\n");
	printf("    %s sync [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("    %s resync [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("    %s synctest [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("    %s resynctest [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("\n");
	printf("    %s copy [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("    %s recopy [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("    %s copytest [dirIzquierda] [dirDerecha]\n", exeFilename);
	printf("    %s recopytest [dirIzquierda] [dirDerecha]\n", exeFilename);
}

FileNode *CheckDir(char *path, int recheck);
int Sync(char *pathLeft, char *pathRight, int recheck, int dryRun);

int main(int argc, char *argv[]) {
	FILE *file;
	unsigned long crc = 0;
	FileTime fileTime;
	int i;

	if (argc < 2) {
		Help(argv[0]);
		return 0;
	}

	if (!strcmp(argv[1], "info") && argc >= 3) {
		// Informacion de ficheros
		for (i = 2; i < argc; i++) {
			if (File_ExistsPath(argv[i])) {
				file = fopen(argv[i], "rb");
				if (file) {
					crc = CRC_File(file);
					fclose(file);
				}
				fileTime = FileTime_Get(argv[i]);
				printf("%s:\t[%08X]\t", argv[i], crc);
				FileTime_Print(fileTime);
				printf("\n");
			}
		}
	} else if (!strcmp(argv[1], "scan") && argc == 4) {
		// Scanear informacion de directorio y guardar arbol
		FileNode *fileNode;
		printf("Building FileNode..\n");
		fileNode = FileNode_Build(argv[2]);
		FileNode_Save(fileNode, argv[3]);
	} else if (!strcmp(argv[1], "rescan") && argc == 4) {
		// Scanear informacion de directorio y guardar arbol
		FileNode *fileNode;
		printf("Loading FileNode..\n");
		fileNode = FileNode_Load(argv[3]);
		if (fileNode) {
			printf("Rebuilding FileNode..\n");
			fileNode = FileNode_Refresh(fileNode, argv[2]);
			FileNode_Save(fileNode, argv[3]);
		}
	} else if (!strcmp(argv[1], "read") && argc == 3) {
		// Leer informacion de arbol
		FileNode *fileNode;
		fileNode = FileNode_Load(argv[2]);
		if (fileNode)
			FileNode_Print(fileNode);
	} else if (!strcmp(argv[1], "dir") && argc == 3) {
		// Leer informacion de dir
		char *path = argv[2];
		char dirNodesFile[MaxPath];
		FileNode *fileNode;

		fileNode = CheckDir(path, 1);
		if (fileNode) {
			FileNode_Print(fileNode);
		}
	} else if (argc == 4) {
		char *cmd = argv[1];
		char *pathLeft = argv[2];
		char *pathRight = argv[3];
		if (!strcmp(cmd, "sync")) {
			Sync(pathLeft, pathRight, 1, 0);
		} else if (!strcmp(cmd, "resync")) {
			Sync(pathLeft, pathRight, 0, 0);
		} else if (!strcmp(cmd, "synctest")) {
			Sync(pathLeft, pathRight, 1, 1);
		} else if (!strcmp(cmd, "resynctest")) {
			Sync(pathLeft, pathRight, 0, 1);
		} else if (!strcmp(cmd, "copy")) {
			Copy(pathLeft, pathRight, 1, 0);
		} else if (!strcmp(cmd, "recopy")) {
			Copy(pathLeft, pathRight, 0, 0);
		} else if (!strcmp(cmd, "copytest")) {
			Copy(pathLeft, pathRight, 1, 1);
		} else if (!strcmp(cmd, "recopytest")) {
			Copy(pathLeft, pathRight, 0, 1);
		}
	} else {
		Help(argv[0]);
	}

	return (0);
}

FileNode *CheckDir(char *path, int recheck) {
	char dirNodesFile[MaxPath];
	FileNode *fileNode;

	// Comprobar directorio
	snprintf(dirNodesFile, MaxPath, "%s/"FileNode_Filename, path);
	if (recheck) {
		printf("Checking Directory.. %s\n", path);
		fileNode = FileNode_Load(dirNodesFile);
		if (fileNode) {
			fileNode = FileNode_Refresh(fileNode, path);
		} else {
			fileNode = FileNode_Build(path);
		}
		FileNode_Save(fileNode, dirNodesFile);
	} else {
		printf("Loading Directory.. %s\n", path);
		fileNode = FileNode_Load(dirNodesFile);
		if (!fileNode) {
			printf("Error, no nodesFile.fs\n");
			return NULL ;
		}
	}
	return fileNode;
}

void PrintStatistics(AccionFileNode *actionFileNode) {
	ActionQueueStatistics statistics;
	AccionFileNode_Statistics(actionFileNode, &statistics);
	printf("Statistics\n");
	printf("       % 12s % 12s % 12s\n", "Read", "Write", "Delete");
	printf("Left : % 12lld % 12lld % 12lld\n", statistics.readLeft,
			statistics.writeLeft, statistics.deleteLeft);
	printf("Right: % 12lld % 12lld % 12lld\n", statistics.readRight,
			statistics.writeRight, statistics.deleteRight);
	printf("\n");
	printf("Copy count     : % 10d\n", statistics.fullCopyCount);
	printf("Date copy count: % 10d\n", statistics.dateCopyCount);
	printf("Directory count: % 10d\n", statistics.directoryCount);
	printf("Delete count   : % 10d\n", statistics.deleteCount);
}

int Sync(char *pathLeft, char *pathRight, int recheck, int dryRun) {
	FileNode *fileNodeLeft, *fileNodeRight;

	// Comprobar y cargar directorios
	if (!File_ExistsPath(pathLeft) || !File_IsDirectory(pathLeft)) {
		printf("Error, directory does not exist: %s\n", pathLeft);
		return 0;
	}
	if (!File_ExistsPath(pathRight) || !File_IsDirectory(pathRight)) {
		printf("Error, directory does not exist: %s\n", pathRight);
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

	// Construir acciones
	printf("Building action list.. \n");
	AccionFileNode *actionFileNode = NULL;
	actionFileNode = AccionFileNode_BuildSync(fileNodeLeft, fileNodeRight);

	if (dryRun) {
		// Mostrar lista de acciones
		AccionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode);
	} else {
		// Ejecutar lista de acciones
		AccionFileNode_RunList(actionFileNode, pathLeft, pathRight);
	}

	return (1);
}

int Copy(char *pathLeft, char *pathRight, int reCheck, int dryRun) {
	FileNode *fileNodeLeft, *fileNodeRight;

	// Comprobar y cargar directorios
	if (!File_ExistsPath(pathLeft) || !File_IsDirectory(pathLeft)) {
		printf("Error, directory does not exist: %s\n", pathLeft);
		return 0;
	}
	if (!File_ExistsPath(pathRight) || !File_IsDirectory(pathRight)) {
		printf("Error, directory does not exist: %s\n", pathRight);
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

	// Construir acciones
	printf("Building action list.. \n");
	AccionFileNode *actionFileNode = NULL;
	actionFileNode = AccionFileNode_BuildCopy(fileNodeLeft, fileNodeRight);

	if (dryRun) {
		// Mostrar lista de acciones
		AccionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode);
	} else {
		// Ejecutar lista de acciones
		AccionFileNode_RunList(actionFileNode, pathLeft, pathRight);
	}

	return (1);
}
