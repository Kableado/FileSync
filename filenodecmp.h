#ifndef _FILENODECMP_H_
#define _FILENODECMP_H_

#include "filenode.h"

typedef enum {
	AccionFileCmp_Nothing,
	AccionFileCmp_LeftToRight,
	AccionFileCmp_RightToLeft,
	AccionFileCmp_DeleteLeft,
	AccionFileCmp_DeleteRight,
	AccionFileCmp_DateLeftToRight,
	AccionFileCmp_DateRightToLeft,
	AccionFileCmp_MakeRightDirectory,
	AccionFileCmp_MakeLeftDirectory
} AccionFileCmp;

typedef struct SAccionFileNode {
	AccionFileCmp action;
	FileNode *left;
	FileNode *right;
	struct SAccionFileNode *next;
} AccionFileNode;

AccionFileNode *AccionFileNode_Create();
void AccionFileNode_Destroy(AccionFileNode *actionFileNode);
AccionFileNode *AccionFileNode_CreateNormal(FileNode *fileNodeLeft,
		FileNode *fileNodeRight);

AccionFileNode *AccionFileNode_BuildSync(FileNode *fileNodeLeft,
		FileNode *fileNodeRight);
AccionFileNode *AccionFileNode_BuildCopy(FileNode *fileNodeLeft,
		FileNode *fileNodeRight);

void AccionFileNode_Print(AccionFileNode *actionFileNode);

void AccionFileNode_RunList(AccionFileNode *actionFileNode, char *pathLeft,
		char *pathRight);

#endif
