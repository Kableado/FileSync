#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <time.h>
#ifdef WIN32
#define _WIN32_WINNT 0x0501
#include <signal.h>
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "fileutil.h"

#ifdef WIN32
long long FileTime_to_POSIX(FILETIME ft) {
	LARGE_INTEGER date, adjust;

	// takes the last modified date
	date.HighPart = ft.dwHighDateTime;
	date.LowPart = ft.dwLowDateTime;

	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000ll * 10000;

	// removes the diff between 1970 and 1601
	date.QuadPart -= adjust.QuadPart;

	// converts back from 100-nanoseconds to seconds
	return date.QuadPart / 10000000ll;
}

FILETIME POSIX_to_FileTime(FileTime ft) {
	LARGE_INTEGER date, adjust;
	FILETIME filetime;

	// converts to 100-nanoseconds from seconds
	date.QuadPart=ft*10000000ll;

	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000ll * 10000ll;

	// removes the diff between 1970 and 1601
	date.QuadPart += adjust.QuadPart;

	// asigns to filetime
	filetime.dwHighDateTime=date.HighPart;
	filetime.dwLowDateTime=date.LowPart;
	return filetime;
}

FileTime FileTime_Get(char *filename) {
	HANDLE hFile;
	FILETIME ftCreate, ftAccess, ftWrite;
	hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
	CloseHandle(hFile);
	return(FileTime_to_POSIX(ftWrite));
}

void FileTime_Set(char *filename,FileTime t) {
	HANDLE hFile;
	FILETIME ftWrite;
	hFile = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	ftWrite=POSIX_to_FileTime(t);
	SetFileTime(hFile, NULL, NULL, &ftWrite);
	CloseHandle(hFile);
}

#else

FileTime FileTime_Get(char *filename) {
	struct stat fs;
	lstat(filename, &fs);
	return (fs.st_mtime);
}

void FileTime_Set(char *filename, FileTime t) {
	struct utimbuf utb;

	utb.actime = t;
	utb.modtime = t;
	utime(filename, &utb);
}

#endif

void FileTime_Print(FileTime t) {
	struct tm *tms;

	tms = localtime((time_t *) &t);
	printf("%04d-%02d-%02d %02d:%02d:%02d", tms->tm_year + 1900,
			tms->tm_mon + 1, tms->tm_mday, tms->tm_hour, tms->tm_min,
			tms->tm_sec);
}

void File_GetName(char *path, char *name) {
	int i, j;

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

int File_ExistePath(char *path) {
	unsigned rc;
	rc=GetFileAttributes(path);

	if(rc==INVALID_FILE_ATTRIBUTES) {
		return(0);
	}
	return(1);
}
int File_EsDirectorio(char *dir) {
	unsigned rc;
	rc=GetFileAttributes(dir);

	if(rc==INVALID_FILE_ATTRIBUTES) {
		return(0);
	}
	if(rc&FILE_ATTRIBUTE_DIRECTORY) {
		return(1);
	}
	return(0);
}
int File_EsFichero(char *fichero) {
	unsigned rc;
	rc=GetFileAttributes(fichero);

	if(rc==INVALID_FILE_ATTRIBUTES) {
		return(0);
	}
	if(rc&FILE_ATTRIBUTE_DIRECTORY) {
		return(0);
	}
	return(1);
}

#else
int File_ExistePath(char *path) {
	struct stat info;

	if (lstat(path, &info) == -1) {
		return (0);
	}
	return (1);
}
int File_EsDirectorio(char *dir) {
	struct stat info;

	if (lstat(dir, &info) == -1) {
		return (0);
	}
	if (S_ISDIR(info.st_mode)) {
		return (1);
	}
	return (0);
}
int File_EsFichero(char *fichero) {
	struct stat info;

	if (lstat(fichero, &info) == -1) {
		return (0);
	}
	if (S_ISDIR(info.st_mode)) {
		return (0);
	}
	return (1);
}
#endif

long long File_TamanhoFichero(char *fichero) {
	FILE *f;
	long long tamanho;

	f = fopen(fichero, "rb");
	if (!f)
		return (-1);

	fseek(f, 0, SEEK_END);
	tamanho = ftell(f);
	fclose(f);
	return (tamanho);
}

#ifdef WIN32
int File_CrearDir(char *path) {
	return(_mkdir(path));
}
#else
int File_CrearDir(char *path) {
	return (mkdir(path, 0777));
}
#endif

#ifdef WIN32

void File_IterateDir(char *path,
		int (*func)(char *path,char *name,void *data),void *data)
{
	int handle;
	struct _finddata_t fileinfo;
	char f_path[MaxPath];
	int fin=0;
	int findnext_rc;
	char path_aux[MaxPath];
	char *ptr;

	snprintf(path_aux,MaxPath,
			"%s/*",path);
	handle=_findfirst(path_aux,&fileinfo);
	if(handle==-1)
	return;

	// Recorrer el directorio
	do {
		if(strcmp(fileinfo.name,".") &&
				strcmp(fileinfo.name,".."))
		{
			// A partir de aqui hay un fichero
			// (o directorio)
			snprintf(f_path,512,
					"%s/%s",path,fileinfo.name);
			fin=func(f_path,fileinfo.name,data);
		}
		findnext_rc=_findnext(handle,&fileinfo);
	}while(findnext_rc!=-1 && !fin);
	_findclose(handle);
}

#else

void File_IterateDir(char *path,
		int (*func)(char *path, char *name, void *data), void *data) {
	DIR *directorio;
	struct dirent *entidad_dir;
	char f_path[MaxPath];
	int fin = 0;
	char *ptr;

	directorio = opendir(path);
	if (directorio == NULL )
		return;

	// Recorrer el directorio
	do {
		entidad_dir = readdir(directorio);
		if (entidad_dir != NULL ) {
			if (strcmp(entidad_dir->d_name, ".")
					&& strcmp(entidad_dir->d_name, "..")) {
				// A partir de aqui hay un fichero
				// (o directorio)
				snprintf(f_path, MaxPath, "%s/%s", path, entidad_dir->d_name);
				fin = func(f_path, entidad_dir->d_name, data);
			}
		}
	} while (entidad_dir != NULL && !fin);
	closedir(directorio);
}
#endif

void File_Borrar(char *path) {
	unlink(path);
}

void File_BorrarDirectorio(char *path) {
	rmdir(path);
}

#define MaxBuffer 16384
int File_Copiar(const char *pathOrig, const char *pathDest) {
	FILE *fOrig, *fDest;
	char buffer[MaxBuffer];
	int readLen = 0;
	int writeLen = 0;
	int ok = 0;

	if ((fOrig = fopen(pathOrig, "rb")) == NULL ) {
		return 0;
	}
	if ((fDest = fopen(pathDest, "wb")) == NULL ) {
		return 0;
	}

	do {
		readLen = fread(&buffer, 1, MaxBuffer, fOrig);
		if (readLen > 0) {
			writeLen = fwrite(&buffer, 1, readLen, fDest);
			if (writeLen != readLen) {
				// Error
				fclose(fOrig);
				fclose(fDest);
				return 0;
			}
		}
	} while (readLen == MaxBuffer);

	if (feof(fOrig)) {
		ok = 1;
	}

	fclose(fOrig);
	fclose(fDest);
	return ok;
}