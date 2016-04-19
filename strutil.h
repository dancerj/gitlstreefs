#if !defined(STRUTIL_H__)
#define STRUTIL_H__
#include <string>
#include <vector>
bool ReadFromFile(int dirfd, const std::string& filename, std::string* result);
std::string ReadFromFileOrDie(int dirfd, const std::string& filename);
std::string PopenAndReadOrDie2(const std::vector<std::string>& command,
			       const std::string* cwd = nullptr,
			       int* maybe_exit_code = nullptr);
#endif
