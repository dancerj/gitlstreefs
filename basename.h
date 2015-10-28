#ifndef __BASENAME_H__
#define __BASENAME_H__
#include <string>

// After the last /.
const std::string BaseName(const std::string n);

// Up to and excluding the last /.
const std::string DirName(const std::string n);

#endif
