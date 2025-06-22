// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

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
#include "p2p.h" // Include the new P2P header
// For FileNode_Filename
#include "filenode.h"

// For signal handling
#include <signal.h>

// Global flag for shutdown
volatile sig_atomic_t shutdown_requested = 0;

void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Print("\nShutdown signal received. Cleaning up...\n");
        shutdown_requested = 1;
    }
}

// Structure to hold data passed to the callback for initial scan of directories
typedef struct {
    char **potentialPaths; // Array of paths that are directories
    int *pathCount;
    int pathCapacity;
} DirScanData;

// Callback for File_IterateDir to list all directories at the top level of a volume
static int ListDirectoriesCallback(char *itemPath, char *itemName, void *data) {
    DirScanData *scanData = (DirScanData *)data;
    if (File_IsDirectory(itemPath)) {
        if (*scanData->pathCount >= scanData->pathCapacity) {
            scanData->pathCapacity = (scanData->pathCapacity == 0) ? 10 : scanData->pathCapacity * 2;
            char **tempPaths = (char **)realloc(scanData->potentialPaths, scanData->pathCapacity * sizeof(char *));
            if (!tempPaths) {
                Print("Error: Memory reallocation failed in ListDirectoriesCallback for potentialPaths\n");
                for(int i=0; i < *scanData->pathCount; i++) { if(scanData->potentialPaths[i]) free(scanData->potentialPaths[i]); }
                free(scanData->potentialPaths);
                scanData->potentialPaths = NULL;
				*scanData->pathCount = 0; // Reset count as we lost data
                return 1; // Stop iteration
            }
            scanData->potentialPaths = tempPaths;
        }
        scanData->potentialPaths[*scanData->pathCount] = strdup(itemPath);
        if (!scanData->potentialPaths[*scanData->pathCount]) {
             Print("Error: strdup failed in ListDirectoriesCallback\n");
             return 1; // Stop iteration
        }
        (*scanData->pathCount)++;
    }
    return 0; // Continue
}

typedef struct {
    char path[MaxPath];
    char volumePath[MaxPath];
    char volumeFsType[MaxFilename];
    char volumeName[MaxFilename];
} ShareableDir;

// Function to find shareable directories
// Returns the count of found directories, or -1 on critical error.
// out_dirs will be allocated and should be freed by the caller.
int FindShareableDirectories(VolumeInfo *volumes, int volumeCount, ShareableDir **out_dirs, int *out_dirCount) {
    ShareableDir *foundDirs = NULL;
    int foundCount = 0;
    int foundCapacity = 0;

    *out_dirs = NULL; // Ensure out_dirs is initialized
    *out_dirCount = 0;

    for (int i = 0; i < volumeCount; i++) {
        Print("Scanning volume: %s (Name: %s, Type: %s)\n", volumes[i].path, volumes[i].name, volumes[i].fsType);

        DirScanData dirScanData = {NULL, 0, 0};
		dirScanData.pathCount = malloc(sizeof(int)); // Allocate memory for pathCount
		if (!dirScanData.pathCount) {
			Print("Error: Failed to allocate memory for pathCount\n");
			// Cleanup any previously allocated foundDirs if necessary
			free(foundDirs);
			return -1;
		}
		*dirScanData.pathCount = 0;


        File_IterateDir(volumes[i].path, ListDirectoriesCallback, &dirScanData);

        for (int j = 0; j < *dirScanData.pathCount; j++) {
            char markerFilePath[MaxPath];
            // Ensure potentialPaths[j] is not NULL before using it
            if (dirScanData.potentialPaths && dirScanData.potentialPaths[j]) {
                snprintf(markerFilePath, MaxPath, "%s/%s", dirScanData.potentialPaths[j], FileNode_Filename);

                if (File_ExistsPath(markerFilePath)) {
                    if (foundCount >= foundCapacity) {
                        foundCapacity = (foundCapacity == 0) ? 5 : foundCapacity * 2;
                        ShareableDir *temp_foundDirs = (ShareableDir *)realloc(foundDirs, foundCapacity * sizeof(ShareableDir));
                        if (!temp_foundDirs) {
                            Print("Error: Memory reallocation failed for foundDirs\n");
                            for(int k=0; k < *dirScanData.pathCount; k++) { if(dirScanData.potentialPaths[k]) free(dirScanData.potentialPaths[k]); }
                            free(dirScanData.potentialPaths);
							free(dirScanData.pathCount);
                            free(foundDirs);
                            *out_dirs = NULL;
                            *out_dirCount = 0;
                            return -1;
                        }
                        foundDirs = temp_foundDirs;
                    }

                    ShareableDir *currentEntry = &foundDirs[foundCount];
                    strncpy(currentEntry->path, dirScanData.potentialPaths[j], MaxPath - 1);
                    currentEntry->path[MaxPath - 1] = '\0';

                    strncpy(currentEntry->volumePath, volumes[i].path, MaxPath - 1);
                    currentEntry->volumePath[MaxPath - 1] = '\0';

                    strncpy(currentEntry->volumeFsType, volumes[i].fsType, MaxFilename - 1);
                    currentEntry->volumeFsType[MaxFilename - 1] = '\0';

                    strncpy(currentEntry->volumeName, volumes[i].name, MaxFilename - 1);
                    currentEntry->volumeName[MaxFilename - 1] = '\0';

                    foundCount++;
                    Print("Found shareable directory: %s on volume %s (FS: %s, VolName: %s)\n",
                          currentEntry->path, currentEntry->volumePath, currentEntry->volumeFsType, currentEntry->volumeName);
                }
                free(dirScanData.potentialPaths[j]);
            }
        }
        free(dirScanData.potentialPaths);
		free(dirScanData.pathCount);
    }

    *out_dirs = foundDirs;
    *out_dirCount = foundCount;
    if (foundCount == 0 && volumeCount > 0) { // No error, but nothing found
        return 0;
    }
    return foundCount; // Number of items found, or -1 if error during allocation
}

// Function prototype for auto-sync (implementation will be below main)
void AutoSyncShareableDirectories(bool reCheck, bool dryRun);
// Function prototype for Sync
int Sync(char *pathLeft, char *pathRight, int reCheck, int dryRun);

typedef struct {
    char *baseName;
    ShareableDir **dirs; // Array of pointers to ShareableDir from the main list
    int count;
    int capacity;
} ShareableGroup;

void AutoSyncShareableDirectories(bool reCheck, bool dryRun) {
    VolumeInfo *volumes = NULL;
    int volumeCount = 0;
    ShareableDir *shareableDirs = NULL;
    int shareableDirCount = 0;

    // 1. Get Volumes
    if (File_GetVolumes(&volumes, &volumeCount) < 0 || volumeCount == 0) {
        Print("No volumes found or error getting volumes.\n");
        if (volumes) free(volumes);
        return;
    }
    Print("Found %d volume(s).\n", volumeCount);

    // 2. Find Shareable Directories
    if (FindShareableDirectories(volumes, volumeCount, &shareableDirs, &shareableDirCount) < 0 || shareableDirCount == 0) {
        Print("No shareable directories found or error finding them.\n");
        free(volumes);
        if (shareableDirs) free(shareableDirs);
        return;
    }
    Print("Found %d shareable director(y/ies) overall.\n", shareableDirCount);

    // 3. Group Shareable Directories by Base Name
    ShareableGroup *groups = NULL;
    int groupCount = 0;
    int groupCapacity = 0;

    for (int i = 0; i < shareableDirCount; i++) {
        char baseNameBuffer[MaxFilename];
        File_GetName(shareableDirs[i].path, baseNameBuffer);
        char *baseName = strdup(baseNameBuffer);
        if (!baseName) {
            Print("Error: strdup failed for baseName from File_GetName buffer.\n");
            continue;
        }
        if (strlen(baseName) == 0) { // Handle cases where File_GetName might return empty for root paths etc.
            Print("Warning: Empty baseName for path %s, skipping group.\n", shareableDirs[i].path);
            free(baseName);
            continue;
        }

        int foundGroupIdx = -1;
        for (int j = 0; j < groupCount; j++) {
            if (strcmp(groups[j].baseName, baseName) == 0) {
                foundGroupIdx = j;
                break;
            }
        }

        if (foundGroupIdx == -1) { // New group
            if (groupCount >= groupCapacity) {
                groupCapacity = (groupCapacity == 0) ? 5 : groupCapacity * 2;
                ShareableGroup *tempGroups = (ShareableGroup *)realloc(groups, groupCapacity * sizeof(ShareableGroup));
                if (!tempGroups) {
                    Print("Error: Failed to realloc groups array.\n");
                    free(baseName);
                    // Free previously allocated groups and their contents
                    for(int k=0; k<groupCount; k++) { free(groups[k].baseName); free(groups[k].dirs); }
                    free(groups);
                    free(shareableDirs);
                    free(volumes);
                    return;
                }
                groups = tempGroups;
            }
            groups[groupCount].baseName = baseName; // baseName is already a strdup
            groups[groupCount].dirs = NULL;
            groups[groupCount].count = 0;
            groups[groupCount].capacity = 0;
            foundGroupIdx = groupCount;
            groupCount++;
        } else {
            free(baseName); // Already have this baseName, free the duplicate
        }

        // Add ShareableDir pointer to the group
        ShareableGroup *currentGroup = &groups[foundGroupIdx];
        if (currentGroup->count >= currentGroup->capacity) {
            currentGroup->capacity = (currentGroup->capacity == 0) ? 2 : currentGroup->capacity * 2;
            ShareableDir **tempDirPointers = (ShareableDir **)realloc(currentGroup->dirs, currentGroup->capacity * sizeof(ShareableDir *));
            if (!tempDirPointers) {
                Print("Error: Failed to realloc dir pointers in group %s.\n", currentGroup->baseName);
                // Error handling here is tricky, might need to cascade cleanup
                // For now, continue, but this group might be incomplete
                continue;
            }
            currentGroup->dirs = tempDirPointers;
        }
        currentGroup->dirs[currentGroup->count] = &shareableDirs[i];
        currentGroup->count++;
    }

    // 4. Perform Synchronization within groups
    Print("Processing synchronization groups...\n");
    for (int i = 0; i < groupCount; i++) {
        ShareableGroup *group = &groups[i];
        Print("Group '%s' has %d director(y/ies):\n", group->baseName, group->count);
        for(int k=0; k < group->count; k++) {
            Print("  - %s (on %s, Vol: %s, FS: %s)\n", group->dirs[k]->path, group->dirs[k]->volumePath, group->dirs[k]->volumeName, group->dirs[k]->volumeFsType);
        }

        if (group->count >= 2) {
            Print("Synchronizing group '%s'...\n", group->baseName);
            for (int j = 0; j < group->count; j++) {
                for (int k = j + 1; k < group->count; k++) {
                    Print("Syncing '%s' <-> '%s'\n", group->dirs[j]->path, group->dirs[k]->path);
                    // The Sync function is bi-directional and handles reCheck/dryRun
                    Sync(group->dirs[j]->path, group->dirs[k]->path, reCheck, dryRun);
                }
            }
        } else {
            Print("Group '%s' has only one directory, no synchronization needed within this group.\n", group->baseName);
        }
    }

    // 5. Cleanup
    Print("Auto-sync process finished. Cleaning up resources.\n");
    for (int i = 0; i < groupCount; i++) {
        free(groups[i].baseName);
        free(groups[i].dirs); // Free the array of pointers
    }
    free(groups);
    free(shareableDirs); // Free the array of ShareableDir structs
    free(volumes);       // Free the array of VolumeInfo structs
}


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

	int postCheckDir = 0;
	long long tRun = Time_GetTime();
	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode, ActionFileNodeResult_Nothing);
	} else {
		// Run action list
		if (ActionFileNode_RunList(actionFileNode, pathLeft, pathRight)) {
			PrintStatistics(actionFileNode, ActionFileNodeResult_Ok);
			postCheckDir = 1;
		}
	}
	tRun = Time_GetTime() - tRun;
	Print("\ttRun:");
	PrintElapsedTime(tRun);
	Print("\n");

	if (postCheckDir) {
		CheckDir(pathLeft, reCheck);
		CheckDir(pathRight, reCheck);
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

	int postCheckDir = 0;
	long long tRun = Time_GetTime();
	if (dryRun) {
		// Show action list
		ActionFileNode_Print(actionFileNode);
		PrintStatistics(actionFileNode, ActionFileNodeResult_Nothing);
	} else {
		// Run action list
		if (ActionFileNode_RunList(actionFileNode, pathLeft, pathRight)) {
			PrintStatistics(actionFileNode, ActionFileNodeResult_Ok);
			postCheckDir = 1;
		}
	}
	tRun = Time_GetTime() - tRun;
	Print("\ttRun:");
	PrintElapsedTime(tRun);
	Print("\n");

	if (postCheckDir) {
		CheckDir(pathLeft, reCheck);
		CheckDir(pathRight, reCheck);
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
	bool AutoSync; // New flag for auto-sync mode
	bool Daemon; // New flag for P2P daemon mode
	char *NodeListFile; // File containing list of known nodes
	int P2PPort;       // TCP port for P2P data communication
	int RescanInterval; // Rescan interval for discovering nodes
    int numDirs;        // Count of directories specified by -dir
	bool NoAction;
	char *Log;
};
TApplicationConfiguration defaultConfig = {
    {NULL}, // Dirs
    false,  // NoScan
    false,  // Dummy
    false,  // Sync
    false,  // Copy
    false,  // AutoSync
    false,  // Daemon
    NULL,   // NodeListFile
    4856,   // P2PPort
    30,     // RescanInterval
    0,      // numDirs
    false,  // NoAction
    NULL    // Log
};

bool SetParam_AutoSync(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->AutoSync = true;
	return true;
}

bool SetParam_Daemon(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->Daemon = true;
	return true;
}

bool SetParam_NodeList(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->NodeListFile = argv[0];
	return true;
}

bool SetParam_P2PPort(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->P2PPort = atoi(argv[0]);
	if (((ApplicationConfiguration)data)->P2PPort <= 0 || ((ApplicationConfiguration)data)->P2PPort > 65535) {
		Print("Error: Invalid P2P port number %s. Must be between 1 and 65535.\n", argv[0]);
		return false;
	}
	return true;
}

bool SetParam_RescanInterval(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->RescanInterval = atoi(argv[0]);
	if (((ApplicationConfiguration)data)->RescanInterval <= 0) {
		Print("Error: Invalid rescan interval %s. Must be a positive integer.\n", argv[0]);
		return false;
	}
	return true;
}

bool SetParam_Dir(int argc, char *argv[], void *data) {
	ApplicationConfiguration config = (ApplicationConfiguration)data;
	if (File_ExistsPath(argv[0]) == 0) {
		Print("Error: Path \"%s\" does not exist.\n", argv[0]);
		return false;
	}
	// Find the next available slot in Dirs, ensuring not to overflow
	int i = 0;
	while (i < (sizeof(config->Dirs)/sizeof(config->Dirs[0])) && config->Dirs[i] != NULL) {
		i++;
	}

	if (i < (sizeof(config->Dirs)/sizeof(config->Dirs[0]))) {
		config->Dirs[i] = argv[0];
        config->numDirs = i + 1; // Update the count of directories
        // Ensure the next one is NULL if we are not at max capacity yet for loops that check NULL
        if ((i + 1) < (sizeof(config->Dirs)/sizeof(config->Dirs[0]))) {
            config->Dirs[i+1] = NULL;
        }
		return true;
	} else {
		Print("Error: Maximum number of directories (%zu) already specified.\n", sizeof(config->Dirs)/sizeof(config->Dirs[0]));
		return false;
	}
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
	{"auto-sync", 0, "Automatically find and synchronize shareable directories across volumes", SetParam_AutoSync},
	{"log", 1, "Log actions to file", SetParam_Log},
	{"daemon", 0, "Run in P2P daemon mode", SetParam_Daemon},
	{"node-list", 1, "Specify a file containing a list of known P2P nodes", SetParam_NodeList},
	{"p2p-port", 1, "Specify the TCP port for P2P data communication (default 4856)", SetParam_P2PPort},
	{"rescan-interval", 1, "Specify the P2P node discovery rescan interval in seconds (default 30)", SetParam_RescanInterval},

	{"scan", 2, "Scan directory and save to filenode file", Func_Scan},
	{"rescan", 2, "Rescan directory and save to filenode file", Func_Rescan},
	{"read", 1, "Read filenode file", Func_Read},
	{"check", 1, "Check changes on a directory", Func_Check},

	{NULL, 0, NULL, NULL},
};

int main(int argc, char *argv[]) {
	TApplicationConfiguration config = defaultConfig;

	Exceptions_Init();

	int parameterParsingResult =
		ParameterOperation_Parse(argc, argv, _parameterOperations, &config);
	if (parameterParsingResult <= 0) {
		ParameterOperation_PrintHelp(_parameterOperations);
		return 0;
	}
    // If NoAction is true, but it's not an auto-sync call (which might use dummy/dryRun via NoAction)
    // or other utility functions that set NoAction, then exit.
	if (config.NoAction && !config.AutoSync &&
        !(parameterParsingResult > 0 && (
            // Check if any of the utility functions (scan, rescan, read, check) were likely called
            // This is a bit heuristic as we don't directly know which function was called by ParameterOperation_Parse
            // We assume if NoAction is set by one of them, it's handled.
            // A cleaner way would be for utility funcs to return a specific status.
            strstr(argv[0], "scan") || strstr(argv[0], "rescan") || strstr(argv[0], "read") || strstr(argv[0], "check")
            // This check is imperfect, as argv[0] is program name.
            // The utility functions Func_Scan etc. set config.NoAction = true.
            // So if NoAction is true, and it's not AutoSync, we assume a utility function ran or help was printed.
        ))
    ) {
        // If NoAction is set by a utility function like 'scan', 'read', 'check', 'rescan',
        // those functions have already done their work.
        // If --noaction was passed explicitly for sync/copy/auto-sync, it's handled by the 'dummy' flag.
        // This logic is mainly to exit if only --noaction is passed without a primary operation.
        // However, ParameterOperation_Parse would return <=0 if only invalid/help options are passed.
        // If a utility func like 'scan' was called, it sets NoAction and does its job.
        // So, if NoAction is true here, and it wasn't auto-sync, it means a utility func ran.
		return 0;
	}


	Print("\n================================ FileSync "
		  "===================================\n");

    if (config.Daemon) {
        Print("Starting P2P Daemon Mode...\n");
        Print("Node List File: %s\n", config.NodeListFile ? config.NodeListFile : "Not specified");
        Print("P2P UDP Discovery Port: %d, P2P TCP Data Port: %d\n", config.P2PPort, config.P2PPort); // Assuming P2PPort is used for both UDP discovery and base for TCP, or make them distinct
        Print("Rescan Interval: %d seconds\n", config.RescanInterval);

        // Initialize P2P Node Discovery (UDP)
        // Note: P2P_InitNodeDiscovery's 'port' argument is for UDP.
        // The TCP server will listen on config.P2PPort (or a dedicated TCP port if defined differently).
        if (!P2P_InitNodeDiscovery(config.P2PPort, config.RescanInterval)) {
            Print("Error: Failed to initialize P2P Node Discovery (UDP).\n");
            return 1; // Indicate an error
        }

        // Initialize P2P TCP Server, providing it with the configured directories
        if (!P2P_InitTCPServer(config.P2PPort, config.Dirs, config.numDirs)) {
            Print("Error: Failed to initialize P2P TCP Server.\n");
            P2P_ShutdownNodeDiscovery(); // Clean up UDP part
            return 1; // Indicate an error
        }

        if (config.NodeListFile) {
            NodeInfo loaded_nodes[10]; // Max 10 nodes from file for this example
            int count = P2P_LoadNodesFromFile(config.NodeListFile, loaded_nodes, 10);
            Print("Loaded %d nodes from file %s\n", count, config.NodeListFile);
            // These nodes are now part of the P2P_GetDiscoveredNodes list if P2P_LoadNodesFromFile adds them
        }

        // Setup signal handling
        signal(SIGINT, handle_signal);
        signal(SIGTERM, handle_signal);

        Print("P2P Daemon running... Press Ctrl+C to exit.\n");
        while(!shutdown_requested) {
            NodeInfo current_nodes[20];
            int node_count = P2P_GetDiscoveredNodes(current_nodes, 20);
            if (node_count > 0 && !shutdown_requested) { // Check shutdown_requested again before heavy processing
                Print("Discovered %d nodes:\n", node_count);
                for (int i = 0; i < node_count; i++) {
                    Print("  Node %d: IP %s, Port %d (TCP)\n", i + 1, current_nodes[i].ip_address, current_nodes[i].port);

                    // Attempt to connect and send a "hello" message
                    int client_socket = P2P_ConnectToNode(current_nodes[i].ip_address, current_nodes[i].port);
                    if (client_socket >= 0) {
                        Print("  Successfully connected to node %s:%d for sync operations.\n", current_nodes[i].ip_address, current_nodes[i].port);

                        // 1. Send REQ_SHARE_LIST
                        if (P2P_SendData(client_socket, P2P_MSG_REQ_SHARE_LIST, strlen(P2P_MSG_REQ_SHARE_LIST))) {
                            Print("    Sent REQ_SHARE_LIST to node.\n");

                            char recv_buffer[4096]; // Buffer for receiving response
                            int bytes_received = P2P_ReceiveData(client_socket, recv_buffer, sizeof(recv_buffer) - 1);
                            if (bytes_received > 0) {
                                recv_buffer[bytes_received] = '\0';
                                Print("    Received from node: %s (len %d)\n", recv_buffer, bytes_received); // Print only first part for brevity if long

                                // 2. Parse SHARE_LIST_RESP
                                if (strncmp(recv_buffer, P2P_MSG_SHARE_LIST_RESP, strlen(P2P_MSG_SHARE_LIST_RESP)) == 0) {
                                    char *p_after_msg_type = recv_buffer + strlen(P2P_MSG_SHARE_LIST_RESP);
                                    if (*p_after_msg_type != ' ') {
                                        Print("    Error: Malformed SHARE_LIST_RESP (missing space after msg type '%s')\n", P2P_MSG_SHARE_LIST_RESP);
                                        P2P_CloseConnection(client_socket); // Close and try next node or iteration
                                        continue;
                                    }
                                    char *num_start_ptr = p_after_msg_type + 1; // Skip space

                                    char *end_ptr_num;
                                    long share_count_long = strtol(num_start_ptr, &end_ptr_num, 10);

                                    if (num_start_ptr == end_ptr_num) { // No digits were read
                                        Print("    Error: Malformed SHARE_LIST_RESP (could not parse count from '%s')\n", num_start_ptr);
                                        P2P_CloseConnection(client_socket);
                                        continue;
                                    }
                                    int share_count = (int)share_count_long;
                                    Print("    Node reported %d shareable directories.\n", share_count);

                                    char* ptr = end_ptr_num; // ptr is now at the start of the first binary int (len_name)
                                    // No space skipping needed here as binary data follows count directly

                                    P2PShareableDir remote_shares[10]; // Max 10 for this example
                                    int parsed_count = 0;
                                    for (int k = 0; k < share_count && parsed_count < 10; k++) {
                                        if (ptr >= recv_buffer + bytes_received) break; // Bounds check

                                        int name_len = 0;
                                        memcpy(&name_len, ptr, sizeof(int));
                                        ptr += sizeof(int);
                                        if (ptr + name_len > recv_buffer + bytes_received) break;
                                        memcpy(remote_shares[parsed_count].name, ptr, name_len);
                                        remote_shares[parsed_count].name[name_len] = '\0';
                                        ptr += name_len;

                                        if (ptr >= recv_buffer + bytes_received) break;
                                        int path_len = 0;
                                        memcpy(&path_len, ptr, sizeof(int));
                                        ptr += sizeof(int);
                                        if (ptr + path_len > recv_buffer + bytes_received) break;
                                        memcpy(remote_shares[parsed_count].path, ptr, path_len);
                                        remote_shares[parsed_count].path[path_len] = '\0';
                                        ptr += path_len;

                                        Print("      Remote Share %d: Name: '%s', Path: '%s'\n", parsed_count + 1, remote_shares[parsed_count].name, remote_shares[parsed_count].path);
                                        parsed_count++;
                                    }

                                    // TODO: Compare with local shares and initiate sync if needed
                                    // This involves calling local FindShareableDirectories, comparing,
                                    // then potentially REQ_FILE_NODE etc.
                                    Print("    Parsed %d share(s) from remote node.\n", parsed_count);

                                    // Client focuses on its first configured directory for P2P matching.
                                    if (config.numDirs > 0 && config.Dirs[0] != NULL) {
                                        char primary_local_dir_path[MaxPath];
                                        strncpy(primary_local_dir_path, config.Dirs[0], MaxPath -1);
                                        primary_local_dir_path[MaxPath-1] = '\0';

                                        char primary_local_dir_name[MaxFilename];
                                        File_GetName(primary_local_dir_path, primary_local_dir_name);

                                        // Check if this primary local dir is actually shareable (has nodesFile.fs)
                                        char marker_check_path[MaxPath];
                                        snprintf(marker_check_path, MaxPath, "%s/%s", primary_local_dir_path, FileNode_Filename);
                                        if (!File_ExistsPath(marker_check_path)){
                                            Print("    Primary local directory %s is not shareable (missing %s).\n", primary_local_dir_path, FileNode_Filename);
                                        } else {
                                            Print("    Client's primary local share for P2P: Name='%s', Path='%s'\n", primary_local_dir_name, primary_local_dir_path);
                                            bool match_found_for_primary = false;
                                            for (int r_idx = 0; r_idx < parsed_count; r_idx++) {
                                                if (strcmp(primary_local_dir_name, remote_shares[r_idx].name) == 0) {
                                                    match_found_for_primary = true;
                                                    if (strcmp(primary_local_dir_path, remote_shares[r_idx].path) != 0) {
                                                        Print("      MATCH FOUND for P2P Sync with primary local dir: '%s'\n", primary_local_dir_name);
                                                        Print("        Local Path : %s\n", primary_local_dir_path);
                                                        Print("        Remote Path: %s\n", remote_shares[r_idx].path);
                                                        // TODO: Initiate actual FileNode exchange and sync logic for this pair.
                                                    } else {
                                                        Print("      Primary local share '%s' has identical path as remote, no P2P sync needed: %s\n", primary_local_dir_name, primary_local_dir_path);
                                                    }
                                                    // Typically, sync one primary local dir with one matching remote dir per peer.
                                                    // If multiple remote shares match the primary local name (unlikely for distinct paths),
                                                    // an additional selection logic might be needed, or just pick the first.
                                                    // For now, we'd act on the first match.
                                                    break; // Found a match for the primary local dir, stop searching remote shares for this peer.
                                                }
                                            }
                                            if (!match_found_for_primary) {
                                                Print("    No remote share found matching client's primary local share '%s'.\n", primary_local_dir_name);
                                            }
                                        }
                                    } else {
                                        Print("    Client has no directories configured with -dir for P2P matching.\n");
                                    }
                                } else if (strncmp(recv_buffer, P2P_MSG_ERROR, strlen(P2P_MSG_ERROR)) == 0) {
                                    Print("    Node responded with error: %s\n", recv_buffer);
                                } else {
                                    Print("    Received unexpected response from node: %s\n", recv_buffer);
                                }
                            } else if (bytes_received == 0) {
                                Print("    Node closed connection gracefully after REQ_SHARE_LIST.\n");
                            } else {
                                Print("    Error receiving data from node after REQ_SHARE_LIST.\n");
                            }
                        } else {
                            Print("    Failed to send REQ_SHARE_LIST to node.\n");
                        }
                        P2P_CloseConnection(client_socket);
                    } else {
                        Print("  Failed to connect to node %s:%d for sync.\n", current_nodes[i].ip_address, current_nodes[i].port);
                    }
                }
            } else {
                Print("No nodes discovered yet.\n");
            }
            // Main daemon loop work would go here, e.g., checking for local file changes,
            // handling incoming requests (if TCP server thread delegates to main thread), etc.
            Print("Daemon main loop iteration complete. Sleeping for 5 seconds...\n");
            Time_Pause(5000 * 1000); // Sleep for 5000ms (5 seconds)
        }

        Print("Shutting down P2P Daemon...\n");
        P2P_ShutdownTCPServer();       // Shutdown TCP server first
        P2P_ShutdownNodeDiscovery();   // Then shutdown UDP discovery
#ifdef _WIN32
        CleanupWindowsSockets(); // Final Winsock cleanup if both are down
#endif
        Print("P2P Daemon stopped.\n");

    } else if (config.AutoSync) {
        Print("Starting Automatic Synchronization...\n");
        // config.NoScan means reCheck=true. config.Dummy is dryRun.
        AutoSyncShareableDirectories(!config.NoScan, config.Dummy);
    } else if (config.Copy || config.Sync) {
        if (config.Dirs[0] == NULL || config.Dirs[1] == NULL) {
            Print("Error: Two directories are needed for copy/sync.\n");
            ParameterOperation_PrintHelp(_parameterOperations);
            return 0;
        }
        if (config.Copy) {
            Copy(config.Dirs[0], config.Dirs[1], !config.NoScan, config.Dummy);
        }
        // Allow both copy and sync if specified, though typically one is chosen.
        if (config.Sync) {
            Sync(config.Dirs[0], config.Dirs[1], !config.NoScan, config.Dummy);
        }
    } else if (!config.NoAction) {
        // This case means no primary action (AutoSync, Copy, Sync) was specified,
        // and NoAction is false (so a utility function that sets NoAction also didn't run).
        Print("Error: Action not specified (e.g., --sync, --copy, --auto-sync, or a utility like --scan).\n");
		ParameterOperation_PrintHelp(_parameterOperations);
        return 0;
    }

	return 0;
}
