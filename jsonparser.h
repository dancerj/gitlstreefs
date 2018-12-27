#ifndef JSON_PARSER_H_
#define JSON_PARSER_H_

#include <map>
#include <memory>
#include <vector>

namespace jjson {

class Value {
public:
  Value() {}
  virtual ~Value() {}

  /** Obtain array element. */
  const Value& operator[](size_t pos) const;

  /** Obtain object member. */
  const Value& operator[](const std::string& key) const;
  /** Obtain object member. */
  const Value& get(const std::string& key) const;

  /** Obtain array for iteration. */
  const std::vector<std::unique_ptr<Value> >& get_array() const;

  /** Obtain string. */
  const std::string& get_string() const;

  /** Obtain number. */
  float get_number() const;

  /** Obtain number and convert to int. */
  float get_int() const;

  /** Check if this was 'true' */
  bool is_true() const;

  /** Check if this was 'false' */
  bool is_false() const;

  /** Check if this was 'null' */
  bool is_null() const;
};

template <class T> class SpecialValue : public Value {
public:
  explicit SpecialValue(T&& value) : value_(std::move(value)) {}
  const T value_;
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


#endif // JSON_PARSER_H_
