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

	if (_free_filenode == NULL) {
		FileNode *nodos;
		int i;
		// Allocate a block
		nodos = malloc(sizeof(FileNode) * FileNode_Block);
		if (nodos == NULL) {
			return NULL;
		}
		for (i = 0; i < FileNode_Block - 1; i++) {
			nodos[i].next = &nodos[i + 1];
		}
		nodos[FileNode_Block - 1].next = NULL;
		_free_filenode = &nodos[0];
	}

	// Get first free
	fileNode = _free_filenode;
	_free_filenode = fileNode->next;
	fileNode->next = NULL;
	_n_filenode++;

	// Initialize
	fileNode->name[0] = 0;
	fileNode->flags = 0;
	fileNode->status = FileStatus_None;
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
	// FIXME: delete childs
}

void FileNode_AddChild(FileNode *fileNode, FileNode *file2) {
	if (!file2 || !fileNode)
		return;
	file2->next = fileNode->child;
	fileNode->child = file2;
	fileNode->childCount++;
	file2->parent = fileNode;
}

void FileNode_SetStatusRec(FileNode *fileNode, FileStatus status) {
	FileNode *fn_child;
	fileNode->status = status;
	fn_child = fileNode->child;
	while (fn_child != NULL) {
		FileNode_SetStatusRec(fn_child, status);
		fn_child = fn_child->next;
	}
}

void FileNode_GetPath_Rec(FileNode *fileNode, char **pathnode) {
	if (fileNode->parent) {
		pathnode[0] = fileNode->parent->name;
		FileNode_GetPath_Rec(fileNode->parent, pathnode + 1);
	}
	else {
		pathnode[0] = NULL;
	}
}
char temppath[MaxPath];
char *FileNode_GetPath(FileNode *fileNode, char *path) {
	char *pathnodes[128];
	int levels, i;
	char *pathptr = temppath;
	if (path) { pathptr = path; }

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

void FileNode_GetSize(FileNode *fileNode, char *file) {
	fileNode->flags |= FileFlag_HasSize;
	fileNode->size = File_GetSize(file);
}

void FileNode_GetTime(FileNode *fileNode, char *file) {
	fileNode->flags |= FileFlag_HasTime;
	fileNode->fileTime = FileTime_Get(file);
}

void FileNode_GetSizeAndTime(FileNode *fileNode, char *file) {
	fileNode->flags |= FileFlag_HasSize | FileFlag_HasTime;
	File_GetSizeAndTime(file, &fileNode->size, &fileNode->fileTime);
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
	short nameLen;

	// Write name
	nameLen = (short)strlen(fileNode->name);
	fwrite((void *)&nameLen, sizeof(nameLen), 1, file);
	if (nameLen>0 && nameLen<MaxFilename) {
		fputs(fileNode->name, file);
	} else {
		return;
	}

	// Write flags
	fwrite((void *)&fileNode->flags, sizeof(fileNode->flags), 1, file);

	// Write status
	fputc((char)fileNode->status, file);

	// Write size
	if (fileNode->flags & FileFlag_HasSize) {
		fwrite((void *)&fileNode->size, sizeof(fileNode->size), 1, file);
	}

	// Write date
	if (fileNode->flags & FileFlag_HasTime) {
		fwrite((void *)&fileNode->fileTime, sizeof(fileNode->fileTime), 1, file);
	}

	// Write CRC
	if (fileNode->flags & FileFlag_HasCRC) {
		fwrite((void *)&fileNode->crc, sizeof(fileNode->crc), 1, file);
	}

	// Write files of directory
	if (fileNode->flags & FileFlag_Directory) {
		FileNode *fileNodeChild;
		fwrite((void *)&fileNode->childCount, sizeof(fileNode->childCount), 1, file);
		fileNodeChild = fileNode->child;
		int cnt = 0;
		while (fileNodeChild) {
			FileNode_SaveNode(fileNodeChild, file);
			fileNodeChild = fileNodeChild->next;
			cnt++;
		}
		if (fileNode->childCount != cnt) {
			return;
		}
	}
}

void FileNode_Save(FileNode *fileNode, char *filePath) {
	FILE *file;
	char mark[5];
	int version;

	if (!fileNode)
		return;
	file = fopen(filePath, "wb+");
	if (!file)
		return;

	// Write mark and version
	strcpy(mark, "sYnC");
	fwrite((void *)mark, sizeof(char), 4, file);
	version = FileNode_Version;
	fwrite((void *)&version, sizeof(int), 1, file);

	FileNode_SaveNode(fileNode, file);
	fclose(file);
}

FileNode *FileNode_LoadNode(FILE *file) {
	short nameLen;
	FileNode *fileNode;
	int i;

	fileNode = FileNode_Create();

	// Read name
	fread((void *)&nameLen, sizeof(nameLen), 1, file);
	fileNode->name[0] = 0;
	if (nameLen<0 || nameLen>MaxFilename) {
		FileNode_Delete(fileNode);
		return NULL;
	}
	if (nameLen > 0) {
		fread((void *)fileNode->name, sizeof(char), nameLen, file);
		fileNode->name[nameLen] = 0;
	}

	// Read flags
	fread((void *)&fileNode->flags, sizeof(fileNode->flags), 1, file);

	// Leer estado
	fileNode->status = fgetc(file);

	// Read status
	if (fileNode->flags & FileFlag_HasSize) {
		fread((void *)&fileNode->size, sizeof(fileNode->size), 1, file);
	}

	// Read date
	if (fileNode->flags & FileFlag_HasTime) {
		fread((void *)&fileNode->fileTime, sizeof(fileNode->fileTime), 1, file);
	}

	// Read CRC
	if (fileNode->flags & FileFlag_HasCRC) {
		fread((void *)&fileNode->crc, sizeof(fileNode->crc), 1, file);
	}

	// Read files on directory
	if (fileNode->flags & FileFlag_Directory) {
		FileNode *fileNodeChildAux = NULL, *fileNodeChild;
		fread((void *)&fileNode->childCount, sizeof(fileNode->childCount), 1, file);
		for (i = 0; i < fileNode->childCount; i++) {
			fileNodeChild = FileNode_LoadNode(file);
			if (fileNodeChild == NULL) {
				// FIXME: Clean memory (fileNode, fileNodeChild etc)
				return NULL;
			}
			fileNodeChild->parent = fileNode;
			if (!fileNodeChildAux) {
				fileNode->child = fileNodeChild;
			}
			else {
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
		return (NULL);

	// Read mark and version
	fread((void *)mark, sizeof(char), 4, file);
	mark[4] = 0;
	if (strcmp(mark, "sYnC")) {
		// Incorrect mark
		fclose(file);
		return (NULL);
	}
	fread((void *)&version, sizeof(int), 1, file);
	if (version != FileNode_Version) {
		// Incorrect version
		fclose(file);
		return (NULL);
	}

	fileNode = FileNode_LoadNode(file);
	fclose(file);

	return (fileNode);
}

void FileNode_PrintNode(FileNode *fileNode) {
	printff(FileNode_GetPath(fileNode, NULL));
	if (fileNode->flags & FileFlag_Normal) {
		printff(" File");
	}
	else {
		printff(" Dir");
	}
	printff(" %d", fileNode->status);
	if (fileNode->status == FileStatus_New) {
		printff(" New");
	}
	if (fileNode->status == FileStatus_Modified) {
		printff(" Modified");
	}
	if (fileNode->status == FileStatus_Deleted) {
		printff(" Deleted!!!");
	}
	printff("\n");

	if (fileNode->flags&FileFlag_HasSize) {
		printff("\\-Size : %lld\n", fileNode->size);
	}
	if (fileNode->flags&FileFlag_HasTime) {
		printff("\\-Date : "); FileTime_Print(fileNode->fileTime); printff("\n");
	}
	if (fileNode->flags&FileFlag_HasCRC) {
		printff("\\-CRC  : [%08X]\n", fileNode->crc);
	}

}

void FileNode_Print(FileNode *fileNode) {
	FileNode *fileNodeAux = fileNode;
	int end = 0;

	while (fileNodeAux != NULL && !end) {

		if (fileNodeAux->parent != NULL) {
			FileNode_PrintNode(fileNodeAux);
		}

		if (fileNodeAux->child) {
			fileNodeAux = fileNodeAux->child;
		}
		else {
			while (fileNodeAux->next == NULL) {
				fileNodeAux = fileNodeAux->parent;
				if (fileNodeAux == fileNode || fileNodeAux == NULL) {
					printff("End\n");
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
		return (NULL);

	// Create node
	fileNode = FileNode_Create();
	File_GetName(path, fileNode->name);

	if (File_IsDirectory(path)) {
		// Get information data from directories, and child files
		fileNode->flags |= FileFlag_Directory;
		FileNode_GetTime(fileNode, path);
		File_IterateDir(path, FileNode_Build_Iterate, fileNode);
	}
	else {
		// Get information data from files
		fileNode->flags |= FileFlag_Normal;
		FileNode_GetSizeAndTime(fileNode, path);
	}

	return (fileNode);
}

int FileNode_Build_Iterate(char *path, char *name, void *d) {
	FileNode *fileNode, *fileNodeParent = d;

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
		FileNode_SetStatusRec(fileNode, FileStatus_Deleted);
		return (fileNode);
	}
	if (!fileNode) {
		// El fichero ha sido creado
		fileNode = FileNode_Build(filePath);
		FileNode_SetStatusRec(fileNode, FileStatus_New);
	}
	else {
		// Comprobar si ha sido modificado
		FileTime fileTime;
		long long size;

		// Marcar normal
		fileNode->status = FileStatus_None;
		fileNode->flags &= ~FileFlag_MarkerForReview;

		// Determinar si es un fichero o directorio
		if (File_IsDirectory(filePath)) {
			FileNode *fileNodeChild;

			// Comparar datos de los directorios
			if (!(fileNode->flags & FileFlag_Directory)) {
				fileNode->status = FileStatus_Modified;
				fileNode->flags |= FileFlag_Directory;
				fileNode->flags &= ~FileFlag_Normal;
			}
			fileTime = FileTime_Get(filePath);
			if (fileTime != fileNode->fileTime) {
				fileNode->status = FileStatus_Modified;
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
					FileNode_SetStatusRec(fileNodeChild, FileStatus_Deleted);
				}
				fileNodeChild = fileNodeChild->next;
			}
		}
		else {
			// Comprar datos de los ficheros
			if (!(fileNode->flags & FileFlag_Normal)) {
				fileNode->status = FileStatus_Modified;
				fileNode->flags |= FileFlag_Normal;
				fileNode->flags &= ~FileFlag_Directory;
			}
			File_GetSizeAndTime(filePath, &size, &fileTime);
			if (size != fileNode->size) {
				fileNode->status = FileStatus_Modified;
				fileNode->size = size;
			}
			if (fileTime != fileNode->fileTime) {
				fileNode->status = FileStatus_Modified;
				fileNode->fileTime = fileTime;
			}
			if (fileNode->status == FileStatus_Modified) {
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
	}
	else {
		// Nuevo, construir
		fileNodeChild = FileNode_Refresh(NULL, path);
		FileNode_AddChild(fileNode, fileNodeChild);
	}

	return (0);
}

