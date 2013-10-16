#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"

FileNode *_free_filenode = NULL;
int _n_filenode = 0;
#define FileNode_Block 1024
FileNode *FileNode_Create() {
	FileNode *fileNode;

	if (_free_filenode == NULL ) {
		FileNode *nodos;
		int i;
		// Reservar un tocho
		nodos = malloc(sizeof(FileNode) * FileNode_Block);
		for (i = 0; i < FileNode_Block - 1; i++) {
			nodos[i].next = &nodos[i + 1];
		}
		nodos[FileNode_Block - 1].next = NULL;
		_free_filenode = &nodos[0];
	}

	// Obtener el primero libre
	fileNode = _free_filenode;
	_free_filenode = fileNode->next;
	_n_filenode++;

	// Iniciar
	fileNode->name[0] = 0;
	fileNode->flags = 0;
	fileNode->estado = FileStatus_None;
	fileNode->size = 0;
	fileNode->crc = 0;
	fileNode->fileTime = 0;
	fileNode->child = NULL;
	fileNode->childCount = 0;
	fileNode->next = NULL;
	fileNode->parent = NULL;

	return (fileNode);
}

void FileNode_Delete(FileNode *fn) {
	fn->next = _free_filenode;
	_free_filenode = fn;
	_n_filenode--;
}

void FileNode_AddChild(FileNode *fileNode, FileNode *file2) {
	if (!file2 || !fileNode)
		return;
	file2->next = fileNode->child;
	fileNode->child = file2;
	fileNode->childCount++;
	file2->parent = fileNode;
}

void FileNode_SetEstadoRec(FileNode *fileNode, FileStatus estado) {
	FileNode *fn_child;
	fileNode->estado = estado;
	fn_child = fileNode->child;
	while (fn_child != NULL ) {
		FileNode_SetEstadoRec(fn_child, estado);
		fn_child = fn_child->next;
	}
}

void FileNode_GetPath_Rec(FileNode *fileNode, char **pathnode) {
	if (fileNode->parent) {
		pathnode[0] = fileNode->parent->name;
		FileNode_GetPath_Rec(fileNode->parent, pathnode + 1);
	} else {
		pathnode[0] = NULL;
	}
}
char temppath[MaxPath];
char *FileNode_GetPath(FileNode *fileNode, char *path) {
	char *pathnodes[128];
	int levels, i;
	char *pathptr = temppath;
	if (path)
		pathptr = path;

	FileNode_GetPath_Rec(fileNode, pathnodes);
	levels = 0;
	while (pathnodes[levels]) {
		levels++;
	}
	strcpy(pathptr, "");
	for (i = levels - 1; i >= 0; i--) {
		strcat(pathptr, pathnodes[i]);
		strcat(pathptr, "/");
	}
	strcat(pathptr, fileNode->name);
	return temppath;
}
char *FileNode_GetFullPath(FileNode *fileNode, char *basePath, char *path) {
	char *pathnodes[128];
	int levels, i;
	char *pathptr = temppath;
	if (path)
		pathptr = path;

	FileNode_GetPath_Rec(fileNode, pathnodes);
	levels = 0;
	while (pathnodes[levels]) {
		levels++;
	}
	strcpy(pathptr, basePath);
	strcat(pathptr, "/");
	for (i = levels - 2; i >= 0; i--) {
		strcat(pathptr, pathnodes[i]);
		strcat(pathptr, "/");
	}
	strcat(pathptr, fileNode->name);
	return temppath;
}

void FileNode_GetSize(FileNode *fileNode, char *filePath) {
	fileNode->flags |= FileFlag_HasSize;
	fileNode->size = File_GetSize(filePath);
}

void FileNode_GetFecha(FileNode *fileNode, char *filePath) {
	fileNode->flags |= FileFlag_HastTime;
	fileNode->fileTime = FileTime_Get(filePath);
}

void FileNode_GetCRC(FileNode *fileNode, char *filePath) {
	FILE *file;
	file = fopen(filePath, "rb");
	if (!file) {
		return;
	}
	fileNode->flags |= FileFlag_HasCRC;
	fileNode->crc = CRC_File(file);
	fclose(file);
}

void FileNode_SaveNode(FileNode *fileNode, FILE *file) {
	short name_len;

	// Escribir nombre
	name_len = strlen(fileNode->name);
	fwrite((void *) &name_len, sizeof(name_len), 1, file);
	fputs(fileNode->name, file);

	// Escribir flags
	fwrite((void *) &fileNode->flags, sizeof(fileNode->flags), 1, file);

	// Escribir estado
	fputc((char) fileNode->estado, file);

	// Escribir tamanho
	if (fileNode->flags & FileFlag_HasSize) {
		fwrite((void *) &fileNode->size, sizeof(fileNode->size), 1, file);
	}

	// Escribir fecha
	if (fileNode->flags & FileFlag_HastTime) {
		fwrite((void *) &fileNode->fileTime, sizeof(fileNode->fileTime), 1, file);
	}

	// Escribir CRC
	if (fileNode->flags & FileFlag_HasCRC) {
		fwrite((void *) &fileNode->crc, sizeof(fileNode->crc), 1, file);
	}

	// Escribir ficheros del directorio
	if (fileNode->flags & FileFlag_Directory) {
		FileNode *fileNodeChild;
		fwrite((void *) &fileNode->childCount, sizeof(fileNode->childCount), 1, file);
		fileNodeChild = fileNode->child;
		while (fileNodeChild) {
			FileNode_SaveNode(fileNodeChild, file);
			fileNodeChild = fileNodeChild->next;
		}
	}
}

void FileNode_Save(FileNode *fileNode, char *filePath) {
	FILE *file;
	char marca[5];
	int version;

	if (!fileNode)
		return;
	file = fopen(filePath, "wb+");
	if (!file)
		return;

	// Escribir marca y version
	strcpy(marca, "sYnC");
	fwrite((void *) marca, sizeof(char), 4, file);
	version = FileNode_Version;
	fwrite((void *) &version, sizeof(int), 1, file);

	FileNode_SaveNode(fileNode, file);
	fclose(file);
}

FileNode *FileNode_LoadNode(FILE *file) {
	short nameLen;
	FileNode *fileNode;
	int i;

	fileNode = FileNode_Create();

	// Leer el nombre
	fread((void *) &nameLen, sizeof(nameLen), 1, file);
	fread((void *) fileNode->name, sizeof(char), nameLen, file);
	fileNode->name[nameLen] = 0;

	// Leer vanderas
	fread((void *) &fileNode->flags, sizeof(fileNode->flags), 1, file);

	// Leer estado
	fileNode->estado = fgetc(file);

	// Leer tamanho
	if (fileNode->flags & FileFlag_HasSize) {
		fread((void *) &fileNode->size, sizeof(fileNode->size), 1, file);
	}

	// Leer fecha
	if (fileNode->flags & FileFlag_HastTime) {
		fread((void *) &fileNode->fileTime, sizeof(fileNode->fileTime), 1, file);
	}

	// Leer CRC
	if (fileNode->flags & FileFlag_HasCRC) {
		fread((void *) &fileNode->crc, sizeof(fileNode->crc), 1, file);
	}

	// Leer ficheros del directorio
	if (fileNode->flags & FileFlag_Directory) {
		FileNode *fileNodeChildAux = NULL, *fileNodeChild;
		fread((void *) &fileNode->childCount, sizeof(fileNode->childCount), 1, file);
		for (i = 0; i < fileNode->childCount; i++) {
			fileNodeChild = FileNode_LoadNode(file);
			fileNodeChild->parent = fileNode;
			if (!fileNodeChildAux) {
				fileNode->child = fileNodeChild;
			} else {
				fileNodeChildAux->next = fileNodeChild;
			}
			fileNodeChildAux = fileNodeChild;
		}
	}

	return (fileNode);
}

FileNode *FileNode_Load(char *filePath) {
	FILE *file;
	FileNode *fileNode;
	char mark[5];
	int version;

	file = fopen(filePath, "rb");
	if (!file)
		return (NULL );

	// Leer marca y version
	fread((void *) mark, sizeof(char), 4, file);
	mark[4] = 0;
	if (strcmp(mark, "sYnC")) {
		// Marca incorrecta
		fclose(file);
		return (NULL );
	}
	fread((void *) &version, sizeof(int), 1, file);
	if (version != FileNode_Version) {
		// Version incorrecta
		fclose(file);
		return (NULL );
	}

	fileNode = FileNode_LoadNode(file);
	fclose(file);

	return (fileNode);
}

void FileNode_PrintNode(FileNode *fileNode) {
	printf(FileNode_GetPath(fileNode, NULL ));
	if (fileNode->flags & FileFlag_Normal) {
		printf(" File");
	} else {
		printf(" Dir");
	}
	printf(" %d", fileNode->estado);
	if (fileNode->estado == FileStatus_New) {
		printf(" Nuevo");
	}
	if (fileNode->estado == FileStatus_Modified) {
		printf(" Modificado");
	}
	if (fileNode->estado == FileStatus_Deleted) {
		printf(" Borrado!!!");
	}
	printf("\n");

	/*
	 // Tamanho
	 if(fn->flags&FileFlag_TieneTamanho){
	 printf("\\-Tamanho: %lld\n",fn->size);
	 }

	 // Fecha
	 if(fn->flags&FileFlag_TieneFecha){
	 printf("\\-Fecha  : ");FileTime_Print(fn->ft);printf("\n");
	 }

	 // CRC
	 if(fn->flags&FileFlag_TieneCRC){
	 printf("\\-CRC    : [%08X]\n",fn->crc);
	 }
	 */
}

void FileNode_Print(FileNode *fileNode) {
	FileNode *fileNodeAux = fileNode;
	int end = 0;

	while (fileNodeAux != NULL && !end) {

		if (fileNodeAux->parent != NULL ) {
			FileNode_PrintNode(fileNodeAux);
		}

		if (fileNodeAux->child) {
			fileNodeAux = fileNodeAux->child;
		} else {
			while (fileNodeAux->next == NULL ) {
				fileNodeAux = fileNodeAux->parent;
				if (fileNodeAux == fileNode || fileNodeAux == NULL ) {
					printf("End\n");
					end = 1;
					break;
				}
			}
			if (!end) {
				fileNodeAux = fileNodeAux->next;
			}
		}
	}
}

int FileNode_Build_Iterate(char *path, char *name, void *d);

FileNode *FileNode_Build(char *path) {
	FileNode *fileNode;

	if (!File_ExistsPath(path))
		return (NULL );

	// Crear el nodo
	fileNode = FileNode_Create();
	File_GetName(path, fileNode->name);

	// Determinar si es un fichero o directorio
	if (File_IsDirectory(path)) {
		// Obtener datos para los directorios
		fileNode->flags |= FileFlag_Directory;
		FileNode_GetFecha(fileNode, path);
		File_IterateDir(path, FileNode_Build_Iterate, fileNode);
	} else {
		// Obtener datos para los ficheros
		fileNode->flags |= FileFlag_Normal;
		FileNode_GetSize(fileNode, path);
		FileNode_GetFecha(fileNode, path);
	}

	return (fileNode);
}

int FileNode_Build_Iterate(char *path, char *name, void *d) {
	FileNode *fileNode, *fileNodeParent = d;
	;

	if (!strcmp(name, FileNode_Filename)) {
		return (0);
	}

	fileNode = FileNode_Build(path);
	FileNode_AddChild(fileNodeParent, fileNode);

	return (0);
}

int FileNode_Refresh_Iterate(char *path, char *name, void *d);

FileNode *FileNode_Refresh(FileNode *fileNode, char *filePath) {
	if (!File_ExistsPath(filePath)) {
		// El fichero/directorio ha sido borrado
		if (!fileNode) {
			fileNode = FileNode_Create();
			File_GetName(filePath, fileNode->name);
		}
		FileNode_SetEstadoRec(fileNode, FileStatus_Deleted);
		return (fileNode);
	}
	if (!fileNode) {
		// El fichero ha sido creado
		fileNode = FileNode_Build(filePath);
		FileNode_SetEstadoRec(fileNode, FileStatus_New);
	} else {
		// Comprobar si ha sido modificado
		FileTime fileTime;
		long long size;
		int crc;

		// Marcar normal
		fileNode->estado = FileStatus_None;
		fileNode->flags &= ~FileFlag_MarkerForReview;

		// Determinar si es un fichero o directorio
		if (File_IsDirectory(filePath)) {
			FileNode *fileNodeChild;

			// Comparar datos de los directorios
			if (!(fileNode->flags & FileFlag_Directory)) {
				fileNode->estado = FileStatus_Modified;
				fileNode->flags |= FileFlag_Directory;
				fileNode->flags &= ~FileFlag_Normal;
			}
			fileTime = FileTime_Get(filePath);
			if (fileTime != fileNode->fileTime) {
				fileNode->estado = FileStatus_Modified;
				fileNode->fileTime = fileTime;
			}

			// Marcar hijos para determinar cual es actualizado
			fileNodeChild = fileNode->child;
			while (fileNodeChild) {
				fileNodeChild->flags |= FileFlag_MarkerForReview;
				fileNodeChild = fileNodeChild->next;
			}

			// Escanear subdirectorios
			File_IterateDir(filePath, FileNode_Refresh_Iterate, fileNode);

			// Buscar que sigan marcados (borrados)
			fileNodeChild = fileNode->child;
			while (fileNodeChild) {
				if (fileNodeChild->flags & FileFlag_MarkerForReview) {
					fileNodeChild->flags &= ~FileFlag_MarkerForReview;
					FileNode_SetEstadoRec(fileNodeChild, FileStatus_Deleted);
				}
				fileNodeChild = fileNodeChild->next;
			}
		} else {
			// Comprar datos de los ficheros
			if (!(fileNode->flags & FileFlag_Normal)) {
				fileNode->estado = FileStatus_Modified;
				fileNode->flags |= FileFlag_Normal;
				fileNode->flags &= ~FileFlag_Directory;
			}
			size = File_GetSize(filePath);
			if (size != fileNode->size) {
				fileNode->estado = FileStatus_Modified;
				fileNode->size = size;
			}
			fileTime = FileTime_Get(filePath);
			if (fileTime != fileNode->fileTime) {
				fileNode->estado = FileStatus_Modified;
				fileNode->fileTime = fileTime;
			}
			if (fileNode->estado == FileStatus_Modified) {
				fileNode->flags &= ~FileFlag_HasCRC;
			}
		}
	}
	return (fileNode);
}

int FileNode_Refresh_Iterate(char *path, char *name, void *d) {
	FileNode *fileNode = d;
	FileNode *fileNodeChild;

	if (!strcmp(name, FileNode_Filename)) {
		return (0);
	}

	// Buscar el fichero entre los del arbol
	fileNodeChild = fileNode->child;
	while (fileNodeChild) {
		if (!strcmp(fileNodeChild->name, name)) {
			break;
		}
		fileNodeChild = fileNodeChild->next;
	}
	if (fileNodeChild) {
		// Existe, refrescar
		FileNode_Refresh(fileNodeChild, path);
	} else {
		// Nuevo, construir
		fileNodeChild = FileNode_Refresh(NULL, path);
		FileNode_AddChild(fileNode, fileNodeChild);
	}

	return (0);
}

