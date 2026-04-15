#pragma once

#include <ostream>

namespace plog {

enum Severity {
  none = 0,
  fatal,
  error,
  warning,
  info,
  debug,
  verbose,
};

class NullStreamBuffer final : public std::streambuf {
 public:
  int overflow(int character) override { return character; }
};

inline NullStreamBuffer& null_buffer() {
  static NullStreamBuffer buffer;
  return buffer;
}

inline std::ostream& null_stream() {
  static std::ostream stream(&null_buffer());
  return stream;
}

}  // namespace plog

#define PLOG_VERBOSE plog::null_stream()
#define PLOG_DEBUG plog::null_stream()
#define PLOG_INFO plog::null_stream()
#define PLOG_WARNING plog::null_stream()
#define PLOG_ERROR plog::null_stream()
#define PLOG_FATAL plog::null_stream()
