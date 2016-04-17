std::string gcrypt_string(const std::string& buf);
void gcrypt_string_get_git_style_relpath(std::string* dir_name,
					 std::string* file_name,
					 const std::string& buf);
bool init_gcrypt();
