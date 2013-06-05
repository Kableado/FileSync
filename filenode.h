#ifndef _FILENODE_H_
#define _FILENODE_H_

#define FileNode_Filename "nodesFile.fs"

#define FileNode_Version 4

#define FileFlag_Raiz 1
#define FileFlag_Normal 2
#define FileFlag_Directorio 4
#define FileFlag_TieneTamanho 8
#define FileFlag_TieneFecha 16
#define FileFlag_TieneCRC 32
#define FileFlag_MarcaRevision 1024

typedef enum {
	EstadoFichero_Nada,
	EstadoFichero_Nuevo,
	EstadoFichero_Modificado,
	EstadoFichero_Borrado
} EstadoFichero;

typedef struct FileNode_Tag{
	char name[MaxFilename];

	int flags;

	EstadoFichero estado;

	long long size;

	unsigned long crc;

	FileTime ft;

	struct FileNode_Tag *child;
	int n_childs;

	struct FileNode_Tag *sig;
	struct FileNode_Tag *padre;
} FileNode;

FileNode *FileNode_New();
void FileNode_Delete(FileNode *fn);
void FileNode_AddChild(FileNode *file,FileNode *file2);

char *FileNode_GetFullPath(FileNode *fn,char *basePath,char *path);

void FileNode_GetTamanho(FileNode *fn,char *file);
void FileNode_GetFecha(FileNode *fn,char *file);
void FileNode_GetCRC(FileNode *fn,char *file);

void FileNode_Save(FileNode *fn,char *fichero);
FileNode *FileNode_Load(char *fichero);

void FileNode_PrintNode(FileNode *fn);
void FileNode_Print(FileNode *fn);


FileNode *FileNode_Build(char *path);

FileNode *FileNode_Refresh(FileNode *file,char *path);


#endif
