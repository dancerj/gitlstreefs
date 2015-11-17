#if !defined(STRUTIL_H__)
#define STRUTIL_H__
#include <string>
#include <vector>
std::string ReadFromFileOrDie(const std::string& filename);
std::string PopenAndReadOrDie2(const std::vector<std::string>& command,
			       const std::string* cwd = nullptr,
			       int* maybe_exit_code = nullptr);
#endif
