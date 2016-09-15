#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"

FileNode _freeFileNode = NULL;
int _n_filenode = 0;
#define FileNode_Block 1024
FileNode FileNode_Create() {
	FileNode fileNode;

	if (_freeFileNode == NULL) {
		FileNode nodes;
		int i;
		// Allocate a block
		nodes = malloc(sizeof(TFileNode) * FileNode_Block);
		if (nodes == NULL) {
			return NULL;
		}
		for (i = 0; i < FileNode_Block - 1; i++) {
			nodes[i].next = &nodes[i + 1];
		}
		nodes[FileNode_Block - 1].next = NULL;
		_freeFileNode = &nodes[0];
	}

	// Get first free
	fileNode = _freeFileNode;
	_freeFileNode = fileNode->next;
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

FileNode FileNode_Copy(FileNode fileNode) {
	FileNode fileNodeNew;

	fileNodeNew = FileNode_Create();

	// Copy
	strcpy(fileNodeNew->name, fileNode->name);
	fileNodeNew->flags = fileNode->flags;
	fileNodeNew->status = fileNode->status;
	fileNodeNew->size = fileNode->size;
	fileNodeNew->crc = fileNode->crc;
	fileNodeNew->fileTime = fileNode->fileTime;

	return fileNodeNew;
}

void FileNode_Delete(FileNode fn) {
	// Delete childs
	FileNode fileNodeChildAux = fn->child;
	FileNode fileNodeChild = fn->child;
	while (fileNodeChild) {
		fn->childCount--;
		fileNodeChildAux = fileNodeChild;
		fileNodeChild = fileNodeChild->next;
		fileNodeChildAux->next = NULL;
		if (fileNodeChildAux->parent == fn) {
			fileNodeChildAux->parent = NULL;
			FileNode_Delete(fileNodeChildAux);
		}
	}

	// Queue freed node
	fn->next = _freeFileNode;
	_freeFileNode = fn;
	_n_filenode--;
}

void FileNode_AddChild(FileNode fileNode, FileNode fileNodeChild) {
	if (!fileNodeChild || !fileNode)
		return;
	fileNodeChild->next = fileNode->child;
	fileNode->child = fileNodeChild;
	fileNode->childCount++;
	fileNodeChild->parent = fileNode;
}

FileNode FileNode_GetRoot(FileNode fileNode) {
	FileNode fileNodeParent = fileNode->parent;
	while (fileNodeParent != NULL && fileNodeParent->parent != NULL) {
		fileNodeParent = fileNodeParent->parent;
	}
	return fileNodeParent;
}

void FileNode_SetStatusRec(FileNode fileNode, FileStatus status) {
	FileNode fileNodeChild;
	if (status == FileStatus_Deleted && fileNode->status != status) {
		FileNode fileNodeRoot = FileNode_GetRoot(fileNode);
		fileNode->fileTime = fileNodeRoot->fileTime;
		;
		fileNode->flags |= FileFlag_HasTime;
	}
	fileNode->status = status;
	fileNodeChild = fileNode->child;
	while (fileNodeChild) {
		FileNode_SetStatusRec(fileNodeChild, status);
		fileNodeChild = fileNodeChild->next;
	}
}

void FileNode_GetPath_Rec(FileNode fileNode, char **pathNode) {
	if (fileNode->parent) {
		pathNode[0] = fileNode->parent->name;
		FileNode_GetPath_Rec(fileNode->parent, pathNode + 1);
	} else {
		pathNode[0] = NULL;
	}
}
char tempPath[MaxPath];
char *FileNode_GetPath(FileNode fileNode, char *path) {
	char *pathNodes[MaxPathNodes];
	int levels, i;
	char *pathPtr = tempPath;
	if (path) {
		pathPtr = path;
	}

	FileNode_GetPath_Rec(fileNode, pathNodes);
	levels = 0;
	while (pathNodes[levels]) {
		levels++;
	}
	strcpy(pathPtr, "");
	for (i = levels - 1; i >= 0; i--) {
		strcat(pathPtr, pathNodes[i]);
		strcat(pathPtr, "/");
	}
	strcat(pathPtr, fileNode->name);
	return tempPath;
}

char *FileNode_GetFullPath(FileNode fileNode, char *basePath, char *path) {
	char *pathNodes[MaxPath];
	int levels, i;
	char *pathPtr = tempPath;
	if (path)
		pathPtr = path;

	FileNode_GetPath_Rec(fileNode, pathNodes);
	levels = 0;
	while (pathNodes[levels]) {
		levels++;
	}
	strcpy(pathPtr, basePath);
	strcat(pathPtr, "/");
	for (i = levels - 2; i >= 0; i--) {
		strcat(pathPtr, pathNodes[i]);
		strcat(pathPtr, "/");
	}
	strcat(pathPtr, fileNode->name);
	return tempPath;
}

void FileNode_LoadSize(FileNode fileNode, char *file) {
	fileNode->flags |= FileFlag_HasSize;
	fileNode->size = File_GetSize(file);
}

void FileNode_LoadTime(FileNode fileNode, char *file) {
	fileNode->flags |= FileFlag_HasTime;
	fileNode->fileTime = FileTime_Get(file);
}

void FileNode_LoadSizeAndTime(FileNode fileNode, char *file) {
	fileNode->flags |= FileFlag_HasSize | FileFlag_HasTime;
	File_GetSizeAndTime(file, &fileNode->size, &fileNode->fileTime);
}

void FileNode_LoadCRC(FileNode fileNode, char *filePath) {
	FILE *file;
	file = fopen(filePath, "rb");
	if (!file) {
		return;
	}
	fileNode->flags |= FileFlag_HasCRC;
	fileNode->crc = CRC_File(file);
	fclose(file);
}

void FileNode_SaveNode(FileNode fileNode, FILE *file) {
	short nameLen;

	// Write name
	nameLen = (short)strlen(fileNode->name);
	fwrite((void *)&nameLen, sizeof(nameLen), 1, file);
	if (nameLen > 0 && nameLen < MaxFilename) {
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
		fwrite((void *)&fileNode->fileTime, sizeof(fileNode->fileTime), 1,
			   file);
	}

	// Write CRC
	if (fileNode->flags & FileFlag_HasCRC) {
		fwrite((void *)&fileNode->crc, sizeof(fileNode->crc), 1, file);
	}

	// Write files of directory
	if (fileNode->flags & FileFlag_Directory) {
		FileNode fileNodeChild;
		fwrite((void *)&fileNode->childCount, sizeof(fileNode->childCount), 1,
			   file);
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

void FileNode_Save(FileNode fileNode, char *filePath) {
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

FileNode FileNode_LoadNode(FILE *file) {
	short nameLen;
	FileNode fileNode;
	int i;

	fileNode = FileNode_Create();

	// Read name
	fread((void *)&nameLen, sizeof(nameLen), 1, file);
	fileNode->name[0] = 0;
	if (nameLen < 0 || nameLen > MaxFilename) {
		FileNode_Delete(fileNode);
		return NULL;
	}
	if (nameLen > 0) {
		fread((void *)fileNode->name, sizeof(char), nameLen, file);
		fileNode->name[nameLen] = 0;
	}

	// Read flags
	fread((void *)&fileNode->flags, sizeof(fileNode->flags), 1, file);

	// Read status
	fileNode->status = fgetc(file);

	// Read status
	if (fileNode->flags & FileFlag_HasSize) {
		fread((void *)&fileNode->size, sizeof(fileNode->size), 1, file);
	}

	// Read date
	if (fileNode->flags & FileFlag_HasTime) {
		fread((void *)&fileNode->fileTime, sizeof(fileNode->fileTime), 1, file);
		if (fileNode->fileTime < 0) {
			fileNode->fileTime = Time_GetCurrentTime();
		}
	}

	// Read CRC
	if (fileNode->flags & FileFlag_HasCRC) {
		fread((void *)&fileNode->crc, sizeof(fileNode->crc), 1, file);
	}

	// Read files on directory
	if (fileNode->flags & FileFlag_Directory) {
		FileNode fileNodeChildAux = NULL;
		FileNode fileNodeChild;
		fread((void *)&fileNode->childCount, sizeof(fileNode->childCount), 1,
			  file);
		for (i = 0; i < fileNode->childCount; i++) {
			fileNodeChild = FileNode_LoadNode(file);
			if (fileNodeChild == NULL) {
				FileNode_Delete(fileNode);
				return NULL;
			}
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

FileNode FileNode_Load(char *filePath) {
	FILE *file;
	FileNode fileNode;
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

void FileNode_PrintNode(FileNode fileNode) {
	Print(FileNode_GetPath(fileNode, NULL));
	if (fileNode->flags & FileFlag_Normal) {
		Print(" File");
	} else {
		Print(" Dir");
	}
	Print(" %d", fileNode->status);
	if (fileNode->status == FileStatus_New) {
		Print(" New");
	}
	if (fileNode->status == FileStatus_Modified) {
		Print(" Modified");
	}
	if (fileNode->status == FileStatus_Deleted) {
		Print(" Deleted!!!");
	}
	Print("\n");

	if (fileNode->flags & FileFlag_HasSize) {
		Print("\\-Size : %lld\n", fileNode->size);
	}
	if (fileNode->flags & FileFlag_HasTime) {
		Print("\\-Date : ");
		FileTime_Print(fileNode->fileTime);
		Print("\n");
	}
	if (fileNode->flags & FileFlag_HasCRC) {
		Print("\\-CRC  : [%08X]\n", fileNode->crc);
	}
}

void FileNode_Print(FileNode fileNode) {
	FileNode fileNodeAux = fileNode;
	int end = 0;

	while (fileNodeAux != NULL && !end) {
		if (fileNodeAux->parent != NULL) {
			FileNode_PrintNode(fileNodeAux);
		}

		if (fileNodeAux->child) {
			fileNodeAux = fileNodeAux->child;
		} else {
			while (fileNodeAux->next == NULL) {
				fileNodeAux = fileNodeAux->parent;
				if (fileNodeAux == fileNode || fileNodeAux == NULL) {
					Print("End\n");
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

FileNode FileNode_Build(char *path) {
	FileNode fileNode;

	if (!File_ExistsPath(path))
		return (NULL);

	// Create node
	fileNode = FileNode_Create();
	File_GetName(path, fileNode->name);
	fileNode->fileTime = Time_GetCurrentTime();
	fileNode->flags |= FileFlag_HasTime;

	if (File_IsDirectory(path)) {
		// Get information data from directories, and child files
		fileNode->flags |= FileFlag_Directory;
		FileNode_LoadTime(fileNode, path);
		File_IterateDir(path, FileNode_Build_Iterate, fileNode);
	} else {
		// Get information data from files
		fileNode->flags |= FileFlag_Normal;
		FileNode_LoadSizeAndTime(fileNode, path);
	}

	return (fileNode);
}

int FileNode_Build_Iterate(char *path, char *name, void *d) {
	FileNode fileNode;
	FileNode fileNodeParent = d;

	if (!strcmp(name, FileNode_Filename)) {
		return (0);
	}

	fileNode = FileNode_Build(path);
	FileNode_AddChild(fileNodeParent, fileNode);

	return (0);
}

int FileNode_Refresh_Iterate(char *path, char *name, void *d);

FileNode FileNode_Refresh(FileNode fileNode, char *filePath) {
	if (!fileNode) {
		Print("FileNode_Refresh: Error NULL FileNode\n");
		return NULL;
	}

	if (!File_ExistsPath(filePath)) {
		// The file/directory has been deleted
		FileNode_SetStatusRec(fileNode, FileStatus_Deleted);
		return (fileNode);
	}

	// Check modification
	FileTime fileTime;
	long long size;

	// Remove mark
	fileNode->flags &= ~FileFlag_MarkerForReview;

	if (File_IsDirectory(filePath)) {
		FileNode fileNodeChild;

		// Check directory data
		if (!(fileNode->flags & FileFlag_Directory)) {
			fileNode->status = FileStatus_Modified;
			fileNode->flags |= FileFlag_Directory;
			fileNode->flags &= ~FileFlag_Normal;
		}
		fileTime = FileTime_Get(filePath);
		if (fileTime != fileNode->fileTime) {
			fileNode->status = FileStatus_Modified;
			fileNode->fileTime = fileTime;
			if (fileNode->fileTime < 0) {
				fileNode->fileTime = Time_GetCurrentTime();
			}
		}

		// Mark childs for review
		fileNodeChild = fileNode->child;
		while (fileNodeChild) {
			fileNodeChild->flags |= FileFlag_MarkerForReview;
			fileNodeChild = fileNodeChild->next;
		}

		// Scan subdirectories
		File_IterateDir(filePath, FileNode_Refresh_Iterate, fileNode);

		// Mark as deleted remaining files marked for review
		fileNodeChild = fileNode->child;
		while (fileNodeChild) {
			if (fileNodeChild->flags & FileFlag_MarkerForReview) {
				fileNodeChild->flags &= ~FileFlag_MarkerForReview;
				FileNode_SetStatusRec(fileNodeChild, FileStatus_Deleted);
			}
			fileNodeChild = fileNodeChild->next;
		}
	} else {
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
			if (fileNode->fileTime < 0) {
				fileNode->fileTime = Time_GetCurrentTime();
			}
		}
		if (fileNode->status == FileStatus_Modified) {
			fileNode->flags &= ~FileFlag_HasCRC;
		}
	}

	// Save update time on root FileNode
	if (fileNode->parent == NULL) {
		fileNode->fileTime = Time_GetCurrentTime();
		fileNode->flags |= FileFlag_HasTime;
	}

	return (fileNode);
}

int FileNode_Refresh_Iterate(char *path, char *name, void *d) {
	FileNode fileNode = d;
	FileNode fileNodeChild;

	if (!strcmp(name, FileNode_Filename)) {
		return (0);
	}

	// Search the file on childs
	fileNodeChild = fileNode->child;
	while (fileNodeChild) {
		if (!strcmp(fileNodeChild->name, name)) {
			break;
		}
		fileNodeChild = fileNodeChild->next;
	}
	if (fileNodeChild) {
		// Exists, Refresh
		FileNode_Refresh(fileNodeChild, path);
	} else {
		// New, Build
		fileNodeChild = FileNode_Build(path);
		FileNode_SetStatusRec(fileNodeChild, FileStatus_New);
		FileNode_AddChild(fileNode, fileNodeChild);
	}

	return (0);
}