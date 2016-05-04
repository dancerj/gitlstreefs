#ifndef RELATIVE_PATH_H_
#define RELATIVE_PATH_H_
// Derive a relative path from FUSE request path useful for looking at
// files relative to a subdirectory. path needs to not be empty
// string.
std::string GetRelativePath(const char* path);

#endif
