#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <set>
#include <vector>

class NinjaBuilder {
public:
  // Parameter for constructor, for specifying default values for rules.
  //
  // Additional linker rules can be specified with CclinkRule.
  struct Config {
    Config() {}

    std::string cxxflags{"-O2 -g --std=c++17 -Wall -Werror -D_FILE_OFFSET_BITS=64 -I."};
    std::string ldflags{""};
    std::string gxx{"g++"};
    std::string gcc{"gcc"};
  };

  explicit NinjaBuilder(const Config& config)
    : data_({"cxxflags = " + config.cxxflags,
	     "ldflags = " + config.ldflags,
	     "gxx = " + config.gxx,
	     "gcc = " + config.gcc,
	     "rule cc",
	     "  command = $gxx -MMD -MF $out.d $cxxflags -c $in -o $out",
	     "  depfile = $out.d",
	     "  deps = gcc",
	     "rule cclink",
	     "  command = $gxx $in -o $out $ldflags",
	     "rule runtest",
	     "  command = ./$in > $out.tmp 2>&1 && mv $out.tmp $out || ( cat $out.tmp; exit 1 )",
	     "rule ninjagenerator",
	     "  command = ./$in",
	     "  generator = true",
	     "build build.ninja: ninjagenerator configure",
	     "build configure.o: cc configure.cc",
	     "build configure: cclink configure.o",}) {
  }

  ~NinjaBuilder() {
    // Emit.
    int fd = open(filename_.c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0666);
    assert(fd != -1);

    for (const auto& line : uniquify_build_lines_) {
      data_.push_back(line);
    }

    std::string joined_data = JoinStrings(data_, "", "", "\n")
      + "\n"; // not sure but maybe I should add the trailing newline.
    int ret = write(fd, joined_data.data(), joined_data.size());
    assert((int)joined_data.size() == ret);
    close(fd);
  }

  void CclinkRule(const char* cclink_name, const char* command) {
    data_.push_back(std::string("rule ") + cclink_name + " ");
    data_.push_back(std::string("  command = ") + command);
  }

  struct CompileLinkObject {

    CompileLinkObject(const CompileLinkObject& c) = delete;
    CompileLinkObject(CompileLinkObject&& c) :
      target_(std::move(c.target_)),
      sources_(std::move(c.sources_)),
      cclink_(std::move(c.cclink_)),
      parent_(std::move(c.parent_)) {}

    ~CompileLinkObject() {
      // link
      parent_->data_.push_back("build " + parent_->outdir_ + target_ + ": " + cclink_ + " "
			       + JoinStrings(sources_, parent_->outdir_, ".o", " "));
      // compile
      for (const auto& source : sources_) {
	parent_->uniquify_build_lines_.insert("build " + parent_->outdir_ + source + ".o" + ": cc " + source + ".cc");
      }
      parent_ = nullptr;
    }

    CompileLinkObject& Cclink(const char* s) {
      cclink_ = std::string(s);
      return *this;
    }

  private:
    friend class NinjaBuilder;

    explicit CompileLinkObject(NinjaBuilder* parent) :
      parent_(parent) {}

    std::string target_{};
    std::vector<std::string> sources_{};
    std::string cclink_{"cclink"};
    NinjaBuilder* parent_;
  };

  CompileLinkObject CompileLinkRunTest(const char* target,
				       std::initializer_list<const char*> sources,
				       std::initializer_list<const char*> extra_test_depends = {}) {
    RunTest(target, extra_test_depends);
    return CompileLink(target, sources);
  }

  CompileLinkObject CompileLink(const char* target, std::initializer_list<const char*> sources) {
    CompileLinkObject c(this);
    c.sources_ = InitializerListToVector(sources);
    c.target_ = target;
    return c;
  }

  void RunTestScript(const char* target,
		     std::initializer_list<const char*> extra_test_depends = {}) {
    std::string test = target;
    std::string stdout = outdir_ + test + ".result";
    data_.push_back("build " + stdout + ": runtest " + test + MaybeExtraDepends(extra_test_depends));
  }

private:
  std::vector<std::string> data_;
  const std::string outdir_{"out/"};
  const std::string filename_{"build.ninja"};
  // build lines that may be emit multiple times and needs to be uniquified.
  std::set<std::string> uniquify_build_lines_{};

  std::string MaybeExtraDepends(const std::initializer_list<const char*>& extra_test_depends) {
    std::string result{};
    if (extra_test_depends.size() > 0) {
      result = result + "| " + JoinStrings(InitializerListToVector(extra_test_depends), "", "", " ");
    }
    return result;
  }

  std::vector<std::string> InitializerListToVector(const std::initializer_list<const char*>& sources) {
    std::vector<std::string> result;
    for (const char* v : sources) {
      result.push_back(v);
    }
    return result;
  }

  void RunTest(const char* target,
	       std::initializer_list<const char*> extra_test_depends = {}) {
    std::string test = outdir_ + target;
    std::string stdout = test + ".result";
    data_.push_back("build " + stdout + ": runtest " + test + MaybeExtraDepends(extra_test_depends));
  }

  static std::string JoinStrings(std::vector<std::string> ss, std::string prefix, std::string postfix, std::string split) {
    std::string result;
    for (size_t i = 0; i < ss.size(); ++i) {
      if (i) result.append(split);
      result.append(prefix + ss[i] + postfix);
    }
    return result;
  }
};
