#include <map>
#include <memory>
#include <vector>

namespace jjson {

class Value {
public:
  Value() {}
  virtual ~Value() {}

  Value& operator[](size_t pos);
  Value& operator[](const std::string& key);
  std::string get_string();
  std::vector<std::unique_ptr<Value> >& get_array();
  float get_number();
  bool is_true();
  bool is_false();
  bool is_null();
};

template <class T> class SpecialValue : public Value {
public:
  SpecialValue(T&& value) : value_(std::move(value)) {}
  // TODO: Do I need accessors?
  T value_;
};

typedef SpecialValue<float> NumberValue;
typedef SpecialValue<std::string> StringValue;;
typedef SpecialValue<std::map<std::string, std::unique_ptr<Value> > > ObjectValue;
typedef SpecialValue<std::vector<std::unique_ptr<Value> > > ArrayValue;;

class TrueValue : public Value {
};

class FalseValue : public Value {
};

class NullValue : public Value {
};

std::unique_ptr<Value> Parse(const std::string& s);

} // end namespace jjson

