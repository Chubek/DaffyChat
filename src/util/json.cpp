#include "daffy/util/json.hpp"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace daffy::util::json {
namespace {

class Parser {
 public:
  explicit Parser(std::string_view input) : input_(input) {}

  core::Result<Value> ParseValue() {
    SkipWhitespace();
    if (IsAtEnd()) {
      return core::Error{core::ErrorCode::kParseError, "Unexpected end of JSON input"};
    }

    const char current = input_[position_];
    if (current == '{') {
      return ParseObject();
    }
    if (current == '[') {
      return ParseArray();
    }
    if (current == '"') {
      auto parsed = ParseString();
      if (!parsed.ok()) {
        return parsed.error();
      }
      return Value(parsed.value());
    }
    if (current == 't' || current == 'f') {
      return ParseBool();
    }
    if (current == 'n') {
      return ParseNull();
    }
    if (current == '-' || std::isdigit(static_cast<unsigned char>(current))) {
      return ParseNumber();
    }

    return core::Error{core::ErrorCode::kParseError, "Unexpected token in JSON input"};
  }

  bool ConsumedAll() {
    SkipWhitespace();
    return IsAtEnd();
  }

 private:
  core::Result<Value> ParseObject() {
    Value::Object object;
    Advance();
    SkipWhitespace();

    if (Match('}')) {
      return Value(std::move(object));
    }

    while (true) {
      auto key = ParseString();
      if (!key.ok()) {
        return key.error();
      }

      SkipWhitespace();
      if (!Match(':')) {
        return core::Error{core::ErrorCode::kParseError, "Expected ':' after object key"};
      }

      auto value = ParseValue();
      if (!value.ok()) {
        return value.error();
      }
      object.emplace(key.value(), value.value());

      SkipWhitespace();
      if (Match('}')) {
        break;
      }
      if (!Match(',')) {
        return core::Error{core::ErrorCode::kParseError, "Expected ',' or '}' in object"};
      }
      SkipWhitespace();
    }

    return Value(std::move(object));
  }

  core::Result<Value> ParseArray() {
    Value::Array array;
    Advance();
    SkipWhitespace();

    if (Match(']')) {
      return Value(std::move(array));
    }

    while (true) {
      auto value = ParseValue();
      if (!value.ok()) {
        return value.error();
      }
      array.push_back(value.value());

      SkipWhitespace();
      if (Match(']')) {
        break;
      }
      if (!Match(',')) {
        return core::Error{core::ErrorCode::kParseError, "Expected ',' or ']' in array"};
      }
      SkipWhitespace();
    }

    return Value(std::move(array));
  }

  core::Result<std::string> ParseString() {
    if (!Match('"')) {
      return core::Error{core::ErrorCode::kParseError, "Expected string literal"};
    }

    std::string result;
    while (!IsAtEnd()) {
      const char current = Advance();
      if (current == '"') {
        return result;
      }
      if (current == '\\') {
        if (IsAtEnd()) {
          return core::Error{core::ErrorCode::kParseError, "Incomplete escape sequence"};
        }
        const char escaped = Advance();
        switch (escaped) {
          case '"':
          case '\\':
          case '/':
            result.push_back(escaped);
            break;
          case 'b':
            result.push_back('\b');
            break;
          case 'f':
            result.push_back('\f');
            break;
          case 'n':
            result.push_back('\n');
            break;
          case 'r':
            result.push_back('\r');
            break;
          case 't':
            result.push_back('\t');
            break;
          default:
            return core::Error{core::ErrorCode::kParseError, "Unsupported escape sequence"};
        }
        continue;
      }
      result.push_back(current);
    }

    return core::Error{core::ErrorCode::kParseError, "Unterminated string literal"};
  }

  core::Result<Value> ParseBool() {
    if (input_.substr(position_, 4) == "true") {
      position_ += 4;
      return Value(true);
    }
    if (input_.substr(position_, 5) == "false") {
      position_ += 5;
      return Value(false);
    }
    return core::Error{core::ErrorCode::kParseError, "Invalid boolean literal"};
  }

  core::Result<Value> ParseNull() {
    if (input_.substr(position_, 4) != "null") {
      return core::Error{core::ErrorCode::kParseError, "Invalid null literal"};
    }
    position_ += 4;
    return Value(nullptr);
  }

  core::Result<Value> ParseNumber() {
    const std::size_t start = position_;
    if (Peek() == '-') {
      Advance();
    }
    while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
      Advance();
    }
    if (!IsAtEnd() && Peek() == '.') {
      Advance();
      while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
        Advance();
      }
    }
    if (!IsAtEnd() && (Peek() == 'e' || Peek() == 'E')) {
      Advance();
      if (!IsAtEnd() && (Peek() == '+' || Peek() == '-')) {
        Advance();
      }
      while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
        Advance();
      }
    }

    const std::string token(input_.substr(start, position_ - start));
    char* end = nullptr;
    const double parsed = std::strtod(token.c_str(), &end);
    if (end == nullptr || *end != '\0') {
      return core::Error{core::ErrorCode::kParseError, "Invalid number literal"};
    }
    return Value(parsed);
  }

  void SkipWhitespace() {
    while (!IsAtEnd() && std::isspace(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
  }

  bool Match(const char expected) {
    if (IsAtEnd() || input_[position_] != expected) {
      return false;
    }
    ++position_;
    return true;
  }

  char Advance() { return input_[position_++]; }
  char Peek() const { return input_[position_]; }
  bool IsAtEnd() const { return position_ >= input_.size(); }

  std::string_view input_;
  std::size_t position_{0};
};

std::string Escape(std::string_view input) {
  std::ostringstream stream;
  for (const char ch : input) {
    switch (ch) {
      case '"':
        stream << "\\\"";
        break;
      case '\\':
        stream << "\\\\";
        break;
      case '\b':
        stream << "\\b";
        break;
      case '\f':
        stream << "\\f";
        break;
      case '\n':
        stream << "\\n";
        break;
      case '\r':
        stream << "\\r";
        break;
      case '\t':
        stream << "\\t";
        break;
      default:
        stream << ch;
        break;
    }
  }
  return stream.str();
}

}  // namespace

Value::Value() : storage_(nullptr) {}
Value::Value(std::nullptr_t value) : storage_(value) {}
Value::Value(bool value) : storage_(value) {}
Value::Value(int value) : storage_(static_cast<double>(value)) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(const char* value) : storage_(std::string(value)) {}
Value::Value(Array value) : storage_(std::move(value)) {}
Value::Value(Object value) : storage_(std::move(value)) {}

bool Value::IsNull() const { return std::holds_alternative<std::nullptr_t>(storage_); }
bool Value::IsBool() const { return std::holds_alternative<bool>(storage_); }
bool Value::IsNumber() const { return std::holds_alternative<double>(storage_); }
bool Value::IsString() const { return std::holds_alternative<std::string>(storage_); }
bool Value::IsArray() const { return std::holds_alternative<Array>(storage_); }
bool Value::IsObject() const { return std::holds_alternative<Object>(storage_); }

bool Value::AsBool() const { return std::get<bool>(storage_); }
double Value::AsNumber() const { return std::get<double>(storage_); }
const std::string& Value::AsString() const { return std::get<std::string>(storage_); }
const Value::Array& Value::AsArray() const { return std::get<Array>(storage_); }
const Value::Object& Value::AsObject() const { return std::get<Object>(storage_); }
Value::Array& Value::AsArray() { return std::get<Array>(storage_); }
Value::Object& Value::AsObject() { return std::get<Object>(storage_); }

const Value* Value::Find(std::string_view key) const {
  if (!IsObject()) {
    return nullptr;
  }
  const auto& object = AsObject();
  const auto it = object.find(std::string(key));
  if (it == object.end()) {
    return nullptr;
  }
  return &it->second;
}

core::Result<Value> Parse(std::string_view input) {
  Parser parser(input);
  auto value = parser.ParseValue();
  if (!value.ok()) {
    return value.error();
  }
  if (!parser.ConsumedAll()) {
    return core::Error{core::ErrorCode::kParseError, "Trailing data after JSON value"};
  }
  return value.value();
}

core::Result<Value> ParseFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return core::Error{core::ErrorCode::kIoError, "Unable to open JSON file: " + path};
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  return Parse(buffer.str());
}

std::string Serialize(const Value& value) {
  if (value.IsNull()) {
    return "null";
  }
  if (value.IsBool()) {
    return value.AsBool() ? "true" : "false";
  }
  if (value.IsNumber()) {
    std::ostringstream stream;
    const double number = value.AsNumber();
    if (std::isfinite(number) && std::floor(number) == number &&
        number >= static_cast<double>(std::numeric_limits<long long>::min()) &&
        number <= static_cast<double>(std::numeric_limits<long long>::max())) {
      stream << static_cast<long long>(number);
    } else {
      stream << std::setprecision(17) << number;
    }
    return stream.str();
  }
  if (value.IsString()) {
    return std::string{"\""} + Escape(value.AsString()) + '"';
  }
  if (value.IsArray()) {
    std::ostringstream stream;
    stream << '[';
    bool first = true;
    for (const auto& entry : value.AsArray()) {
      if (!first) {
        stream << ',';
      }
      first = false;
      stream << Serialize(entry);
    }
    stream << ']';
    return stream.str();
  }

  std::ostringstream stream;
  stream << '{';
  bool first = true;
  for (const auto& [key, entry] : value.AsObject()) {
    if (!first) {
      stream << ',';
    }
    first = false;
    stream << '"' << Escape(key) << '"' << ':' << Serialize(entry);
  }
  stream << '}';
  return stream.str();
}

}  // namespace daffy::util::json
