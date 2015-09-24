#if !defined(STRUTIL_H__)
#define STRUTIL_H__
#include <string>
std::string ReadFromFileOrDie(const std::string& filename);
std::string PopenAndReadOrDie(const std::string& command,
			      int* maybe_exit_code = nullptr);
#endif
