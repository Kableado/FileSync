#ifndef _ACTIONFILENODESYNC_H_
#define _ACTIONFILENODESYNC_H_

#include "actionfilenode.h"
#include "filenode.h"

ActionFileNode ActionFileNode_BuildSync(FileNode fileNodeLeft,
										FileNode fileNodeRight);

#endif
