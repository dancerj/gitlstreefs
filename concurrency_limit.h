#include <condition_variable>
#include <mutex>

class ScopedConcurrencyLimit {
public:
  ScopedConcurrencyLimit(); 
  ~ScopedConcurrencyLimit();
private:
  static constexpr size_t kLimit{6};
  static size_t jobs_;
  static std::mutex m_;
  static std::condition_variable cv_;
};
