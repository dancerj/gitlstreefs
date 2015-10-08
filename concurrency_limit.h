#include <condition_variable>
#include <mutex>
#include <set>

class ScopedConcurrencyLimit {
public:
  ScopedConcurrencyLimit(const std::string& message);
  ~ScopedConcurrencyLimit();
private:
  void DumpStatus();
  static constexpr size_t kLimit{6};
  std::string message_;
  static std::mutex m_;
  static std::condition_variable cv_;
  static std::set<const std::string*> messages_;
};
