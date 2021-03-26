/*
 * Adapter for FUSE operations and directory_container.
 */

#ifndef GIT_ADAPTER_H_
#define GIT_ADAPTER_H_
#include <fuse.h>
#include "directory_container.h"

namespace git_adapter {
directory_container::DirectoryContainer* GetDirectoryContainer();
fuse_operations GetFuseOperations();
}  // namespace git_adapter
#endif
