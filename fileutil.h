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
#define MaxFilename 512

void File_GetName(char *path, char *name);

int File_ExistePath(char *path);

int File_EsDirectorio(char *path);

int File_EsFichero(char *path);

long long File_TamanhoFichero(char *ficheros);

int File_CrearDir(char *path);

void File_IterateDir(char *path,
		int (*func)(char *path, char *name, void *data), void *data);

void File_Borrar(char *path);
void File_BorrarDirectorio(char *path);

#endif
