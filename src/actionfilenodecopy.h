// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez

#ifndef _ACTIONFILENODECOPY_H_
#define _ACTIONFILENODECOPY_H_

#include "actionfilenode.h"
#include "filenode.h"

ActionFileNode ActionFileNode_BuildCopy(FileNode fileNodeLeft,
										FileNode fileNodeRight);

#endif
