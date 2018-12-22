/*
 * Adapter for FUSE operations and directory_container.
 */

#ifndef GIT_ADAPTER__
#define GIT_ADAPTER__
#include "directory_container.h"
#include <fuse.h>

namespace git_adapter {
directory_container::DirectoryContainer* GetDirectoryContainer();
fuse_operations GetFuseOperations();
}
#endif
