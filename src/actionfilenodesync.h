#ifndef _ACTIONFILENODESYNC_H_
#define _ACTIONFILENODESYNC_H_

#include "filenode.h"
#include "actionfilenode.h"

ActionFileNode ActionFileNode_BuildSync(FileNode fileNodeLeft,
	FileNode fileNodeRight);

#endif
