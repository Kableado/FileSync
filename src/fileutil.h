// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

#ifndef _FILEUTIL_
#define _FILEUTIL_

////////////////////////////////////////////////
// FileTime

typedef long long FileTime;

/////////////////////////////
// FileTime_Get
//
// Gets the current time in POSIX.
FileTime FileTime_Get(char *filename);

/////////////////////////////
// FileTime_Set
//
// Sets the current time in POSIX.
void FileTime_Set(char *filename, FileTime t);

/////////////////////////////
// FileTime_Print
//
// Prints the filetime
void FileTime_Print(FileTime t);

///////////////////////////////////////////////
// File
#define MaxFilename 2048
#define MaxPath 4096
#define MaxPathNodes 512

long long File_GetSize(char *fileName);

void File_GetSizeAndTime(char *fileName, long long *size, FileTime *time);

void File_GetName(char *path, char *name);

int File_ExistsPath(char *path);

int File_IsDirectory(char *path);

int File_IsFile(char *path);

int File_MakeDirectory(char *path);

void File_IterateDir(char *path,
					 int (*func)(char *path, char *name, void *data),
					 void *data);

int File_Delete(char *path);
int File_DeleteDirectory(char *path);

int File_Copy(const char *pathOrig, const char *pathDest);

///////////////////////////////////////////////
// Volume Information

typedef struct {
	char path[MaxPath];
	char name[MaxFilename]; // Volume label or device name
	char fsType[MaxFilename]; // Filesystem type
} VolumeInfo;

// Populates the 'volumes' array with information about connected volumes.
// Returns the number of volumes found, or a negative value on error.
// The caller is responsible for freeing the 'volumes' array using free().
int File_GetVolumes(VolumeInfo **volumes, int *count);

#endif
