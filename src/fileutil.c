
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#endif

#include "fileutil.h"
#include "util.h"

#ifdef WIN32
long long FileTime_to_POSIX(FILETIME fileTime) {
	LARGE_INTEGER date, adjust;

	// takes the last modified date
	date.HighPart = fileTime.dwHighDateTime;
	date.LowPart = fileTime.dwLowDateTime;

	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000ll * 10000;

	// removes the diff between 1970 and 1601
	date.QuadPart -= adjust.QuadPart;

	// converts back from 100-nanoseconds to seconds
	return date.QuadPart / 10000000ll;
}

FILETIME POSIX_to_FileTime(FileTime fileTime) {
	LARGE_INTEGER date, adjust;
	FILETIME fileTimeOut;

	// converts to 100-nanoseconds from seconds
	date.QuadPart = fileTime * 10000000ll;

	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000ll * 10000ll;

	// removes the diff between 1970 and 1601
	date.QuadPart += adjust.QuadPart;

	// asigns to filetime
	fileTimeOut.dwHighDateTime = date.HighPart;
	fileTimeOut.dwLowDateTime = date.LowPart;
	return fileTimeOut;
}

/////////////////////////////
// FileTime_Get
//
// Gets the current time in POSIX.
FileTime FileTime_Get(char *fileName) {
	HANDLE hFile;
	FILETIME ftCreate, ftAccess, ftWrite;
	hFile = CreateFile(fileName, READ_CONTROL, FILE_SHARE_READ, NULL,
					   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
	CloseHandle(hFile);
	return (FileTime_to_POSIX(ftWrite));
}

/////////////////////////////
// FileTime_Set
//
// Sets the current time in POSIX.
void FileTime_Set(char *fileName, FileTime fileTime) {
	HANDLE hFile;
	FILETIME ftWrite;
	hFile = CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
					   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	ftWrite = POSIX_to_FileTime(fileTime);
	SetFileTime(hFile, NULL, NULL, &ftWrite);
	CloseHandle(hFile);
}

#else

/////////////////////////////
// FileTime_Get
//
// Gets the current time in POSIX.
FileTime FileTime_Get(char *fileName) {
	struct stat fs;
	lstat(fileName, &fs);
	return (fs.st_mtime);
}

/////////////////////////////
// FileTime_Set
//
// Sets the current time in POSIX.
void FileTime_Set(char *fileName, FileTime t) {
	struct utimbuf utb;

	utb.actime = t;
	utb.modtime = t;
	utime(fileName, &utb);
}

#endif

/////////////////////////////
// FileTime_Print
//
// Prints the filetime
void FileTime_Print(FileTime fileTime) {
	struct tm *tms;

	tms = localtime((time_t *)&fileTime);
	Print("%04d-%02d-%02d %02d:%02d:%02d", tms->tm_year + 1900, tms->tm_mon + 1,
		  tms->tm_mday, tms->tm_hour, tms->tm_min, tms->tm_sec);
}

#ifdef WIN32

long long File_GetSize(char *fileName) {
	HANDLE hFile;
	DWORD fSize;
	hFile = CreateFile(fileName, READ_CONTROL, FILE_SHARE_READ, NULL,
					   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	fSize = GetFileSize(hFile, NULL);
	CloseHandle(hFile);
	return (fSize);
}
#else

long long File_GetSize(char *fileName) {
	struct stat fs;
	lstat(fileName, &fs);
	return (fs.st_size);
}
#endif

#ifdef WIN32
void File_GetSizeAndTime(char *fileName, long long *size, FileTime *time) {
	HANDLE hFile;
	DWORD fSize;
	FILETIME ftCreate, ftAccess, ftWrite;
	hFile = CreateFile(fileName, READ_CONTROL, FILE_SHARE_READ, NULL,
					   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	fSize = GetFileSize(hFile, NULL);
	GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
	CloseHandle(hFile);
	*size = fSize;
	*time = FileTime_to_POSIX(ftWrite);
}
#else
void File_GetSizeAndTime(char *fileName, long long *size, FileTime *time) {
	struct stat fs;
	lstat(fileName, &fs);
	*size = fs.st_size;
	*time = fs.st_mtime;
}
#endif

void File_GetName(char *path, char *name) {
	size_t i, j;

	i = strlen(path) - 1;
	while (i >= 0) {
		if (path[i] == '/' || path[i] == '\\') {
			i++;
			break;
		} else {
			i--;
		}
	}
	if (i < 0)
		i++;

	j = 0;
	while (path[i]) {
		name[j] = path[i];
		i++;
		j++;
	}
	name[j] = 0;
}

#ifdef WIN32

int File_ExistsPath(char *path) {
	unsigned rc;
	rc = GetFileAttributes(path);

	if (rc == INVALID_FILE_ATTRIBUTES) {
		SetError(strerror(errno));
		return (0);
	}
	return (1);
}
int File_IsDirectory(char *fileName) {
	unsigned rc;
	rc = GetFileAttributes(fileName);

	if (rc == INVALID_FILE_ATTRIBUTES) {
		SetError(strerror(errno));
		return (0);
	}
	if (rc & FILE_ATTRIBUTE_DIRECTORY) {
		return (1);
	}
	return (0);
}
int File_IsFile(char *fileName) {
	unsigned rc;
	rc = GetFileAttributes(fileName);

	if (rc == INVALID_FILE_ATTRIBUTES) {
		SetError(strerror(errno));
		return (0);
	}
	if (rc & FILE_ATTRIBUTE_DIRECTORY) {
		return (0);
	}
	return (1);
}

#else
int File_ExistsPath(char *path) {
	struct stat info;

	if (lstat(path, &info) == -1) {
		return (0);
	}
	return (1);
}
int File_IsDirectory(char *fileName) {
	struct stat info;

	if (lstat(fileName, &info) == -1) {
		SetError(strerror(errno));
		return (0);
	}
	if (S_ISDIR(info.st_mode)) {
		return (1);
	}
	return (0);
}
int File_IsFile(char *fileName) {
	struct stat info;

	if (lstat(fileName, &info) == -1) {
		SetError(strerror(errno));
		return (0);
	}
	if (S_ISDIR(info.st_mode)) {
		return (0);
	}
	return (1);
}
#endif

#ifdef WIN32
int File_MakeDirectory(char *path) { return (CreateDirectory(path, NULL)); }
#else
int File_MakeDirectory(char *path) {
	int rc = mkdir(path, 0777);
	if (rc != 0) {
		SetError(strerror(errno));
		return 0;
	}
	return 1;
}
#endif

#ifdef WIN32

void File_IterateDir(char *path,
					 int (*func)(char *path, char *name, void *data),
					 void *data) {
	intptr_t handle;
	struct _finddata_t fileinfo;
	char f_path[MaxPath];
	int fin = 0;
	int findnext_rc;
	char path_aux[MaxPath];

	snprintf(path_aux, MaxPath, "%s/*", path);
	handle = _findfirst(path_aux, &fileinfo);
	if (handle == -1)
		return;

	// Iterate directory
	do {
		if (strcmp(fileinfo.name, ".") && strcmp(fileinfo.name, "..")) {
			// Each item
			snprintf(f_path, 512, "%s/%s", path, fileinfo.name);
			fin = func(f_path, fileinfo.name, data);
		}
		findnext_rc = _findnext(handle, &fileinfo);
	} while (findnext_rc != -1 && !fin);
	_findclose(handle);
}

#else

void File_IterateDir(char *path,
					 int (*func)(char *path, char *name, void *data),
					 void *data) {
	DIR *directorio;
	struct dirent *entidad_dir;
	char f_path[MaxPath];
	int fin = 0;
	char *ptr;

	directorio = opendir(path);
	if (directorio == NULL)
		return;

	// Iterate directory
	do {
		entidad_dir = readdir(directorio);
		if (entidad_dir != NULL) {
			if (strcmp(entidad_dir->d_name, ".") &&
				strcmp(entidad_dir->d_name, "..")) {
				// Each item
				snprintf(f_path, MaxPath, "%s/%s", path, entidad_dir->d_name);
				fin = func(f_path, entidad_dir->d_name, data);
			}
		}
	} while (entidad_dir != NULL && !fin);
	closedir(directorio);
}
#endif

int File_Delete(char *path) {
#ifdef WIN32
	int rc = remove(path);
#else
	int rc = unlink(path);
#endif
	if (rc != 0) {
		SetError(strerror(errno));
		return 0;
	}
	return 1;
}

int File_DeleteDirectory(char *path) {
#ifndef WIN32
	int rc = rmdir(path);
#else
	int rc = _rmdir(path);
#endif
	if (rc != 0) {
		SetError(strerror(errno));
		return 0;
	}
	return 1;
}

#define MaxBuffer 16384
int File_Copy(const char *pathOrig, const char *pathDest) {
	FILE *fOrig = NULL;
	FILE *fDest = NULL;
	char *buffer = NULL;
	size_t readLen = 0;
	size_t writeLen = 0;
	int status = 0;

	if ((fOrig = fopen(pathOrig, "rb")) == NULL) {
		SetError(strerror(errno));
		goto cleanup;
	}
	if ((fDest = fopen(pathDest, "wb")) == NULL) {
		SetError(strerror(errno));
		goto cleanup;
	}

	buffer = (char *)malloc(sizeof(char) * MaxBuffer);
	if (buffer == NULL) {
		goto cleanup;
	}

	do {
		readLen = fread(buffer, 1, MaxBuffer, fOrig);
		if (readLen > 0) {
			writeLen = fwrite(buffer, 1, readLen, fDest);
			if (writeLen != readLen) {
				SetError("Write error");
				goto cleanup;
			}
		}
	} while (readLen == MaxBuffer);

	if (feof(fOrig)) {
		status = 1;
	}

cleanup:
	if (fOrig != NULL) {
		fclose(fOrig);
	}
	if (fDest != NULL) {
		fclose(fDest);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	return status;
}