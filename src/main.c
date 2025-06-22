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
// For FileNode_Filename
#include "filenode.h"

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
	bool NoAction;
	char *Log;
};
TApplicationConfiguration defaultConfig = {{NULL}, false, false, false,
										   false,  false, false, NULL};

bool SetParam_AutoSync(int argc, char *argv[], void *data) {
	((ApplicationConfiguration)data)->AutoSync = true;
	return true;
}

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
	{"auto-sync", 0, "Automatically find and synchronize shareable directories across volumes", SetParam_AutoSync},
	{"log", 1, "Log actions to file", SetParam_Log},

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

    if (config.AutoSync) {
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
