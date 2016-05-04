#ifndef FILE_COPY_H_
#define FILE_COPY_H_
bool FileCopyInternal(int dirfd, int from_fd, const struct stat& st, const std::string& target);
bool FileCopy(int dirfd, const std::string& source, const std::string& target);
#endif
