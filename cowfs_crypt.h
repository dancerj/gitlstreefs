#ifndef COWFS_CRYPT_H_
#define COWFS_CRYPT_H_
std::string gcrypt_string(const std::string& buf);
void gcrypt_string_get_git_style_relpath(std::string* dir_name,
                                         std::string* file_name,
                                         const std::string& buf);
bool init_gcrypt();
#endif
