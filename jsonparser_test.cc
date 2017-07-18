#include "jsonparser.h"

#include <assert.h>

#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <vector>

using jjson::Value;

template<class T> void utilTestKeywordParse(const std::string& w) {
  std::unique_ptr<Value> v = jjson::Parse(w);
  assert(v.get() != nullptr);
  assert(dynamic_cast<T*>(v.get()) != nullptr);
}

template<class T, class U> void utilTestValueParse(const std::string json,
						   const U& value) {
  std::unique_ptr<Value> v = jjson::Parse(json);
  assert(v.get() != nullptr);
  T* target = dynamic_cast<T*>(v.get());
  assert(target != nullptr);
  // std::cout << target->value_ << std::endl;
  assert(target->value_ == value);
}

template<class T, class U> void utilTestArrayParse(const std::string json,
						   const std::vector<U>& values) {
  std::unique_ptr<Value> v = jjson::Parse(json);
  assert(v.get() != nullptr);
  jjson::ArrayValue* target = dynamic_cast<jjson::ArrayValue*>(v.get());
  assert(target != nullptr);
  for (size_t i = 0; i < values.size(); ++i) {
    T* value = dynamic_cast<T*>(target->value_[i].get());
    assert(value != nullptr);
    assert(value->value_ == values[i]);
  }
}


void testConsume() {
  {
    std::string space_tab = " \t\ntrue\n\t";
    jjson::TrueValue* t = dynamic_cast<jjson::TrueValue*>(jjson::Parse(space_tab).get());
    assert(t != nullptr);
  }

  utilTestKeywordParse<jjson::TrueValue>("true");
  utilTestKeywordParse<jjson::FalseValue>("false");
  utilTestKeywordParse<jjson::NullValue>("null");

  assert(jjson::Parse("true")->is_true());
  assert(!jjson::Parse("true")->is_false());
  assert(!jjson::Parse("true")->is_null());
  assert(!jjson::Parse("false")->is_true());
  assert(jjson::Parse("false")->is_false());
  assert(!jjson::Parse("false")->is_null());
  assert(!jjson::Parse("null")->is_true());
  assert(!jjson::Parse("null")->is_false());
  assert(jjson::Parse("null")->is_null());

  utilTestValueParse<jjson::NumberValue>("1", 1);
  utilTestValueParse<jjson::NumberValue>("100", 100);
  utilTestValueParse<jjson::NumberValue>("-10", -10);
  utilTestValueParse<jjson::NumberValue>("5.5", 5.5);

  utilTestValueParse<jjson::StringValue>("\"5.5\"", "5.5");
  utilTestValueParse<jjson::StringValue>("\"unkotest\"", "unkotest");
  utilTestValueParse<jjson::StringValue>("\"carriage\\r\\nreturn\"", "carriage\r\nreturn");
  utilTestValueParse<jjson::StringValue>("\"\\u0075\"", "u");

  utilTestArrayParse<jjson::NumberValue, float>("[1, 2, 3]",
						{1, 2, 3});
  utilTestArrayParse<jjson::NumberValue, float>("[ ]",
						{ });


  {
    std::unique_ptr<Value> v = jjson::Parse("{\"string\" : \"hoge\", \"number\" : 123}");
    assert(v.get() != nullptr);
    auto target = dynamic_cast<jjson::ObjectValue*>(v.get());
    assert(target != nullptr);
    assert(dynamic_cast<jjson::StringValue*>(target->value_.find("string")->second.get())->value_ == "hoge");
    assert(dynamic_cast<jjson::NumberValue*>(target->value_.find("number")->second.get())->value_ == 123);
  }


  {
    std::unique_ptr<Value> v = jjson::Parse("{\"obj\" : {\"hoge\" : 12 }, \"arr\" : [1, 2, 3]}");
    assert(v.get() != nullptr);
    auto target = dynamic_cast<jjson::ObjectValue*>(v.get());
    assert(target != nullptr);
    auto num = dynamic_cast<jjson::NumberValue*>(dynamic_cast<jjson::ObjectValue*>(target->value_.find("obj")->second.get())->value_.find("hoge")->second.get());
    assert(num->value_ == 12);
    auto& arr = dynamic_cast<jjson::ArrayValue*>(target->value_.find("arr")->second.get())->value_;
    assert(dynamic_cast<jjson::NumberValue*>(arr[0].get())->value_ == 1);
    assert(dynamic_cast<jjson::NumberValue*>(arr[1].get())->value_ == 2);
    assert(dynamic_cast<jjson::NumberValue*>(arr[2].get())->value_ == 3);
  }

  {
    std::unique_ptr<Value> v = jjson::Parse("{\"obj\" : {\"hoge\" : 12, \"fuga\": \"sss\" }, \"arr\" : [1, 2, 3]}");
    assert(v.get() != nullptr);
    assert((*v)["obj"]["hoge"].get_number() == 12);
    assert((*v)["obj"]["fuga"].get_string() == "sss");
    assert((*v)["arr"][0].get_number() == 1);
    assert((*v)["arr"][1].get_number() == 2);
    assert((*v)["arr"][2].get_number() == 3);
  }
}

int main(int ac, char** av) {
  testConsume();
}
