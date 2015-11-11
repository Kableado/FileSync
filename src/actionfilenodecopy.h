#ifndef _ACTIONFILENODECOPY_H_
#define _ACTIONFILENODECOPY_H_

#include "filenode.h"
#include "actionfilenode.h"

ActionFileNode ActionFileNode_BuildCopy(FileNode fileNodeLeft,
	FileNode fileNodeRight);

#endif
