// Parser for RFC7159 JSON with some missing implementations.
#include "jsonparser.h"

#include <assert.h>

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace jjson {

const Value& Value::operator[](size_t pos) const {
  return *dynamic_cast<const ArrayValue*>(this)->value_.at(pos).get();
}

const Value& Value::operator[](const std::string& key) const {
  return *dynamic_cast<const ObjectValue*>(this)->value_.find(key)->second;
}

const std::vector<std::unique_ptr<Value> >& Value::get_array() const {
  return dynamic_cast<const ArrayValue*>(this)->value_;
}

const Value& Value::get(const std::string& key) const {
  const auto o = dynamic_cast<const ObjectValue*>(this);
  assert(o != nullptr);
  const auto& it = o->value_.find(key);
  assert(it != o->value_.end());
  const auto& n = it->second;
  assert(n.get() != nullptr);
  return *n;
}

const std::string& Value::get_string() const {
  const auto s = dynamic_cast<const StringValue*>(this);
  assert(s != nullptr);
  return s->value_;
}

float Value::get_number() const {
  const auto n = dynamic_cast<const NumberValue*>(this);
  assert(n != nullptr);
  return n->value_;
}

float Value::get_int() const {
  assert(this != nullptr);
  const auto n = dynamic_cast<const NumberValue*>(this);
  assert(n != nullptr);
  return static_cast<int>(n->value_);
}

bool Value::is_true() const {
  return dynamic_cast<const TrueValue*>(this) != nullptr;
}
bool Value::is_false() const {
  return dynamic_cast<const FalseValue*>(this) != nullptr;
}
bool Value::is_null() const {
  return dynamic_cast<const NullValue*>(this) != nullptr;
}

namespace {
class Parser {
  // Internal class used from Parse() method to do the actual parsing.
public:
  explicit Parser(const std::string& s) : text_(s) {}
  ~Parser() {}

  std::unique_ptr<Value> ParseJsonText() {
    SkipWhitespace();
    std::unique_ptr<Value> v = ObtainValue();
    SkipWhitespace();
    return v;
  }

private:
  char Peek() const {
    if (Eof()) return 0;
    return text_[position_];
  }

  // Returns true on usual case, if there's exceptional case of EOF
  // already, returns false.
  bool Skip() {
    if (Eof()) return false;
    position_++;
    return true;
  }

  char Get() {
    if (Eof()) return 0;
    return text_[position_++];
  }

  bool Eof() const {
    return position_ >= text_.size();
  }

  void ReportError(const std::string& error) {
    // TODO: do useful error reporting.
    std::cout << "Error: " << error << " at " <<
      position_ << " in [" << text_.substr(position_) << "]" << std::endl;
  }

  std::string Consume(const std::set<char>& valid) {
    std::string data{};
    data.reserve(16);
    while (valid.find(Peek()) != valid.end()) {
      data += Peek();
      if (!Skip()) break;
    }
    // Replacing with substr didn't improve speed.
    return data;
  }

  void SkipValid(const std::set<char>& valid) {
    while (valid.find(Peek()) != valid.end()) {
      if (!Skip()) break;
    }
  }

  const std::set<char> valid_whitespace {
    0x20, 0x09, 0x0a, 0x0d};

  void SkipWhitespace() {
    SkipValid(valid_whitespace);
  }

  const std::set<char> valid_number {
    '-', '+', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', 'E'};

  std::unique_ptr<Value> ObtainNumber() {
    std::string number = Consume(valid_number);
    // TODO: implement proper handling of JSON number format.
    return std::make_unique<NumberValue>(strtof(number.c_str(), nullptr));
  }

  std::unique_ptr<Value> ObtainObject() {
    if (Get() != '{') {
      ReportError("Unexpected error.");
      return nullptr;
    }
    SkipWhitespace();
    std::map<std::string, std::unique_ptr<Value> > object;
    while(!Eof()) {
      if (Peek() == '}') {
	Skip();
	return std::make_unique<ObjectValue>(std::move(object));
      }
      auto key = ObtainString();
      SkipWhitespace();
      if (Get() != ':') {
	ReportError("':' expected in Object.");
	return nullptr;
      }
      SkipWhitespace();
      auto value = ObtainValue();
      SkipWhitespace();

      object.emplace(std::move(dynamic_cast<StringValue*>(key.get())->value_), move(value));
      if (Peek() == '}') {
	continue;
      }
      if (Peek() == ',') {
	Skip();
	SkipWhitespace();
	continue;
      }
      ReportError("',' or '}' expected in Object.");
    }
    ReportError("Unexpected end reached while parsing Object.");
    return nullptr;
  }

  std::unique_ptr<Value> ObtainArray() {
    if (Get() != '[') {
      return nullptr;
    }
    SkipWhitespace();
    std::vector<std::unique_ptr<Value> > array;

    while(!Eof()) {
      if (Peek() == ']') {
	Skip();
	return std::make_unique<ArrayValue>(std::move(array));
      }

      std::unique_ptr<Value> value = ObtainValue();
      if (value.get() == nullptr) {
	ReportError("Invalid value parsing array.");
	return nullptr;
      }
      array.emplace_back(move(value));
      SkipWhitespace();
      switch(Peek()) {
      case ']':
	continue;
      case ',':
	Skip();
	SkipWhitespace();
	continue;
      default:
	ReportError("Unexpected char during array.");
	return nullptr;
      }
    }
    ReportError("Unexpected end in array.");
    return nullptr;
  }

  std::unique_ptr<Value> ObtainString() {
    std::string s{};
    // Just to reduce the number of reallocations, try a random large enough buffer.
    s.reserve(1024);
    if (Get() != '"') return nullptr;
    while(!Eof()) {
      if (Peek() == '"') {
	// End of string.
	Skip();
	// TODO: some kind of char code conversion needed?
	return std::make_unique<StringValue>(std::move(s));
      } else if (Peek() == '\\') {
	// escaped character.
	Skip();
	switch(char c = Get()) {
	case 0x22:
	case 0x5c:
	case 0x2f:
	  s += c;
	  break;
	case 0x62:
	  s+= 0x8;
	  break;
	case 0x66:
	  s+= 0xc;
	  break;
	case 0x6e:
	  s+= 0xa;
	  break;
	case 0x72:
	  s+= 0xd;
	  break;
	case 0x74:
	  s+= 0x9;
	  break;
	case 0x75:
	  // 4 hex digits. TODO implement properly.
	  char buf[5] = {Get(), Get(), Get(), Get(), 0};
	  long charcode = strtol(buf, nullptr, 16);
	  if (charcode >= 0 && charcode < 256) {
	    s += static_cast<char>(charcode);
	  } else {
	    ReportError("Unimplemented.");
	    return nullptr;
	  }
	  break;
	}
      } else {
	s += Get();
      }
    }
    ReportError("Unexpected EOF found on string parsing.");
    return nullptr;
  }

  template <class T> std::unique_ptr<Value>
  AssertConsume(const std::string& expect) {
    for (char c: expect) {
      if (c != Peek()) {
	return nullptr;
      }
      if (!Skip()) return nullptr;
    }
    return std::make_unique<T>();
  }

  std::unique_ptr<Value> ObtainValue() {
    switch (Peek()) {
    case 'f':
      return AssertConsume<FalseValue>("false");
    case 'n':
      return AssertConsume<NullValue>("null");
    case 't':
      return AssertConsume<TrueValue>("true");
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '+':
      return ObtainNumber();

    case '[':
      return ObtainArray();

    case '{':
      return ObtainObject();

    case '"':
      return ObtainString();
    }
    return nullptr;
  }

  const std::string& text_;
  size_t position_{0};
};
}  // anonymous namespace

std::unique_ptr<Value> Parse(const std::string& s) {
  Parser p(s);
  return p.ParseJsonText();
}

}  // namespace jjson

