#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "actionfilenode.h"
#include "actionfilenodesync.h"

#define maxDeltaTime 4000

#define QueueNode(queue,node) (queue)->next = (node); (queue) = (node);


void AccionFileNode_CopyPair(FileNode fileNodeLeft, FileNode fileNodeRight,
	ActionFileNode *actionFileNodeQueue)
{
	ActionFileNode actionFileNodeNew = ActionFileNode_CreateNormal(
		fileNodeLeft, fileNodeRight);

	if (!fileNodeLeft && !fileNodeRight) {
		AccionFileNode_Destroy(actionFileNodeNew);
		return;
	}
	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
				AccionFileNode_DeletePair);
		}

		if (fileNodeRight->status != FileStatus_Deleted) {
			actionFileNodeNew->action = ActionFileCmp_DeleteRight;
		}
		else {
			actionFileNodeNew->action = ActionFileCmp_Nothing;
		}
		QueueNode(*actionFileNodeQueue, actionFileNodeNew);
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->status != FileStatus_Deleted) {
			if (fileNodeLeft->flags & FileFlag_Directory) {
				actionFileNodeNew->action = ActionFileCmp_MakeRightDirectory;
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;
				AccionFileNode_CompareChilds(actionFileNodeNew,
					actionFileNodeQueue, AccionFileNode_CopyPair);
				actionFileNodeNew = ActionFileNode_CreateNormal(fileNodeLeft,
					fileNodeRight);
				actionFileNodeNew->action = ActionFileCmp_DateLeftToRight;
			}
			else {
				actionFileNodeNew->action = ActionFileCmp_LeftToRight;
			}
		}
		else {
			actionFileNodeNew->action = ActionFileCmp_Nothing;
		}
		QueueNode(*actionFileNodeQueue, actionFileNodeNew);
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory)
			|| (fileNodeRight->flags & FileFlag_Directory))
		{
			if (fileNodeLeft->status != FileStatus_Deleted) {
				AccionFileNode_CompareChilds(actionFileNodeNew,
					actionFileNodeQueue, AccionFileNode_CopyPair);
				if (abs((int)(fileNodeLeft->fileTime - fileNodeRight->fileTime))
					<= maxDeltaTime) { // appox. equal
					actionFileNodeNew->action = ActionFileCmp_Nothing;
				}
				else {
					actionFileNodeNew->action = ActionFileCmp_DateLeftToRight;
				}
			}
			else {
				AccionFileNode_CompareChilds(actionFileNodeNew,
					actionFileNodeQueue, AccionFileNode_DeletePair);
				if (fileNodeRight->status != FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteRight;
				}
			}
		}
		else {
			if (fileNodeLeft->status != FileStatus_Deleted) {
				if (abs((int)(fileNodeLeft->fileTime - fileNodeRight->fileTime))
					<= maxDeltaTime)
				{
					actionFileNodeNew->action = ActionFileCmp_Nothing;
				}
				else {
					actionFileNodeNew->action = ActionFileCmp_LeftToRight;
				}
			}
			else {
				if (fileNodeRight->status != FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteRight;
				}
			}
		}
		QueueNode(*actionFileNodeQueue, actionFileNodeNew);
	}
}

ActionFileNode ActionFileNode_BuildCopy(FileNode fileNodeLeft,
	FileNode fileNodeRight) {
	ActionFileNode actionFileNodeRoot = ActionFileNode_CreateNormal(
		fileNodeLeft, fileNodeRight);
	ActionFileNode actionFileNodeQueue = actionFileNodeRoot;
	AccionFileNode_CompareChilds(actionFileNodeRoot, &actionFileNodeQueue,
		AccionFileNode_CopyPair);
	return actionFileNodeRoot;
}

