bool FileCopyInternal(int dirfd, int from_fd, const struct stat& st, const std::string& target);
bool FileCopy(int dirfd, const std::string& source, const std::string& target);
