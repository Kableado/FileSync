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
	int(*func)(char *path, char *name, void *data), void *data);

void File_Delete(char *path);
void File_DeleteDirectory(char *path);

int File_Copy(const char *pathOrig, const char *pathDest);

#endif
