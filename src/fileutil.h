#ifndef _FILEUTIL_
#define _FILEUTIL_

////////////////////////////////////////////////
// FileTime

typedef long long FileTime;

FileTime FileTime_Get(char *filename);
void FileTime_Set(char *filename, FileTime t);
void FileTime_Print(FileTime t);

///////////////////////////////////////////////
// File
#define MaxPath 4096
#define MaxFilename 2048

void File_GetName(char *path, char *name);

int File_ExistsPath(char *path);

int File_IsDirectory(char *path);

int File_IsFile(char *path);

long long File_GetSize(char *fileName);

void File_GetSizeAndTime(char *fileName, long long *size, FileTime *time);

int File_MakeDirectory(char *path);

void File_IterateDir(char *path,
	int(*func)(char *path, char *name, void *data), void *data);

void File_Delete(char *path);
void File_DeleteDirectory(char *path);

int File_Copy(const char *pathOrig, const char *pathDest);

#endif
