#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "daffy/core/error.hpp"

namespace daffy::util::json {

class Value {
 public:
  using Object = std::map<std::string, Value>;
  using Array = std::vector<Value>;
  using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

  Value();
  Value(std::nullptr_t value);
  Value(bool value);
  Value(int value);
  Value(double value);
  Value(std::string value);
  Value(const char* value);
  Value(Array value);
  Value(Object value);

  [[nodiscard]] bool IsNull() const;
  [[nodiscard]] bool IsBool() const;
  [[nodiscard]] bool IsNumber() const;
  [[nodiscard]] bool IsString() const;
  [[nodiscard]] bool IsArray() const;
  [[nodiscard]] bool IsObject() const;

  [[nodiscard]] bool AsBool() const;
  [[nodiscard]] double AsNumber() const;
  [[nodiscard]] const std::string& AsString() const;
  [[nodiscard]] const Array& AsArray() const;
  [[nodiscard]] const Object& AsObject() const;
  [[nodiscard]] Array& AsArray();
  [[nodiscard]] Object& AsObject();

  [[nodiscard]] const Value* Find(std::string_view key) const;

 private:
  Storage storage_;
};

core::Result<Value> Parse(std::string_view input);
core::Result<Value> ParseFile(const std::string& path);
std::string Serialize(const Value& value);

}  // namespace daffy::util::json
