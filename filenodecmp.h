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

typedef struct SActionQueueStatistics {
	long long readLeft;
	long long writeLeft;
	long long readRight;
	long long writeRight;
	int fullCopyCount;
	int dateCopyCount;
	int directoryCount;
	int deleteCount;
	long long deleteLeft;
	long long deleteRight;
} ActionQueueStatistics;

void AccionFileNode_Statistics(AccionFileNode *actionFileNode,
		ActionQueueStatistics *statistics);

void AccionFileNode_Print(AccionFileNode *actionFileNode);

void AccionFileNode_RunList(AccionFileNode *actionFileNode, char *pathLeft,
		char *pathRight);

#endif
