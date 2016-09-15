#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "actionfilenode.h"
#include "actionfilenodesync.h"

#define MaxDeltaTime 0

#define QueueNode(queue, node)                                                 \
	(queue)->next = (node);                                                    \
	(queue) = (node);

void AccionFileNode_SyncPair(FileNode fileNodeLeft, FileNode fileNodeRight,
							 ActionFileNode *actionFileNodeQueue) {
	if (!fileNodeLeft && !fileNodeRight) {
		return;
	}
	ActionFileNode actionFileNodeNew =
		ActionFileNode_CreateNormal(fileNodeLeft, fileNodeRight);

	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			// Directory
			if (fileNodeRight->status == FileStatus_Deleted) {
				actionFileNodeNew->action = ActionFileCmp_Nothing;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			} else {
				actionFileNodeNew->action = ActionFileCmp_MakeLeftDirectory;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);

				// Iterate childs
				AccionFileNode_CompareChilds(actionFileNodeNew,
											 actionFileNodeQueue,
											 AccionFileNode_SyncPair);

				// Create new action for date copy
				actionFileNodeNew =
					ActionFileNode_CreateNormal(fileNodeLeft, fileNodeRight);
				actionFileNodeNew->action = ActionFileCmp_DateRightToLeft;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			}
		} else {
			// File
			if (fileNodeRight->status == FileStatus_Deleted) {
				actionFileNodeNew->action = ActionFileCmp_Nothing;
			} else {
				actionFileNodeNew->action = ActionFileCmp_RightToLeft;
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		}
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->flags & FileFlag_Directory) {
			// Directory
			if (fileNodeLeft->status == FileStatus_Deleted) {
				actionFileNodeNew->action = ActionFileCmp_Nothing;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			} else {
				actionFileNodeNew->action = ActionFileCmp_MakeRightDirectory;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);

				// Iterate childs
				AccionFileNode_CompareChilds(actionFileNodeNew,
											 actionFileNodeQueue,
											 AccionFileNode_SyncPair);

				// Create new action for date copy
				actionFileNodeNew =
					ActionFileNode_CreateNormal(fileNodeLeft, fileNodeRight);
				actionFileNodeNew->action = ActionFileCmp_DateLeftToRight;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			}
		} else {
			// File
			if (fileNodeLeft->status == FileStatus_Deleted) {
				actionFileNodeNew->action = ActionFileCmp_Nothing;
			} else {
				actionFileNodeNew->action = ActionFileCmp_LeftToRight;
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		}
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory) &&
			(fileNodeRight->flags & FileFlag_Directory)) {
			// Directory

			// Prepare action for directory pair
			if (abs((int)(fileNodeLeft->fileTime - fileNodeRight->fileTime)) <=
				MaxDeltaTime) { // aprox. equal
				if (fileNodeRight->status == FileStatus_Deleted &&
					fileNodeLeft->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_Nothing;
				} else if (fileNodeRight->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteLeft;
				} else if (fileNodeLeft->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteRight;
				} else {
					actionFileNodeNew->action = ActionFileCmp_Nothing;
				}
			} else if (fileNodeLeft->fileTime < fileNodeRight->fileTime) {
				if (fileNodeRight->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteLeft;
					if (fileNodeLeft->status == FileStatus_Deleted) {
						actionFileNodeNew->action = ActionFileCmp_Nothing;
					}
				} else {
					if (fileNodeLeft->status == FileStatus_Deleted) {
						actionFileNodeNew->action =
							ActionFileCmp_MakeLeftDirectory;
						QueueNode(*actionFileNodeQueue, actionFileNodeNew);
						actionFileNodeNew = ActionFileNode_CreateNormal(
							fileNodeLeft, fileNodeRight);
					}
					actionFileNodeNew->action = ActionFileCmp_DateRightToLeft;
				}
			} else if (fileNodeLeft->fileTime > fileNodeRight->fileTime) {
				if (fileNodeLeft->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteRight;
					if (fileNodeRight->status == FileStatus_Deleted) {
						actionFileNodeNew->action = ActionFileCmp_Nothing;
					}
				} else {
					if (fileNodeRight->status == FileStatus_Deleted) {
						actionFileNodeNew->action =
							ActionFileCmp_MakeRightDirectory;
						QueueNode(*actionFileNodeQueue, actionFileNodeNew);
						actionFileNodeNew = ActionFileNode_CreateNormal(
							fileNodeLeft, fileNodeRight);
					}
					actionFileNodeNew->action = ActionFileCmp_DateLeftToRight;
				}
			}

			// Process child nodes
			if (actionFileNodeNew->action == ActionFileCmp_DeleteRight ||
				actionFileNodeNew->action == ActionFileCmp_DeleteLeft) {
				// Iterate child nodes for deletion
				AccionFileNode_CompareChilds(actionFileNodeNew,
											 actionFileNodeQueue,
											 AccionFileNode_DeletePair);
			} else {
				AccionFileNode_CompareChilds(actionFileNodeNew,
											 actionFileNodeQueue,
											 AccionFileNode_SyncPair);
			}

			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		} else if ((fileNodeLeft->flags & FileFlag_Normal) &&
				   (fileNodeRight->flags & FileFlag_Normal)) {
			// Files

			// Prepare action for file pair
			if (abs((int)(fileNodeLeft->fileTime - fileNodeRight->fileTime)) <=
				MaxDeltaTime) { // aprox. equal
				if (fileNodeRight->status == FileStatus_Deleted &&
					fileNodeLeft->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_Nothing;
				} else if (fileNodeRight->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteLeft;
				} else if (fileNodeLeft->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteRight;
				} else {
					actionFileNodeNew->action = ActionFileCmp_Nothing;
				}
			} else if (fileNodeLeft->fileTime < fileNodeRight->fileTime) {
				// FIXME: Check size to determine y further checks are necessary
				actionFileNodeNew->action = ActionFileCmp_RightToLeft;
				if (fileNodeRight->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteLeft;
					if (fileNodeLeft->status == FileStatus_Deleted) {
						actionFileNodeNew->action = ActionFileCmp_Nothing;
					}
				}
			} else if (fileNodeLeft->fileTime > fileNodeRight->fileTime) {
				// FIXME: Check size to determine y further checks are necessary
				actionFileNodeNew->action = ActionFileCmp_LeftToRight;
				if (fileNodeLeft->status == FileStatus_Deleted) {
					actionFileNodeNew->action = ActionFileCmp_DeleteRight;
					if (fileNodeRight->status == FileStatus_Deleted) {
						actionFileNodeNew->action = ActionFileCmp_Nothing;
					}
				}
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		} else {
			// FIXME: !!!!!
			// Directory vs File
		}
	}
}

ActionFileNode ActionFileNode_BuildSync(FileNode izquierda, FileNode derecha) {
	ActionFileNode actionFileNodeRoot =
		ActionFileNode_CreateNormal(izquierda, derecha);
	ActionFileNode actionFileNodeQueue = actionFileNodeRoot;
	AccionFileNode_CompareChilds(actionFileNodeRoot, &actionFileNodeQueue,
								 AccionFileNode_SyncPair);
	return actionFileNodeRoot;
}