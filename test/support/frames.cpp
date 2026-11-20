/// @file
/// @brief The sample frames, a frame writer and scripted streams (R-TEST-7).

#include "frames.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/copy.hpp>
#include <turborel/error.hpp>
#include <turborel/result.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifndef _WIN32
#include <stdlib.h> // NOLINT(modernize-deprecated-headers): POSIX's setenv and unsetenv, which <cstdlib> need not declare.
#endif
#include <fstream>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test {

namespace {

/// A hexadecimal digit's value.
unsigned hexit(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<unsigned>(c - '0');
  }
  return static_cast<unsigned>(c - 'A' + 10);
}

/// The octets of upper-case hexadecimal digits.
bytes unhex(std::string_view hex) {
  bytes out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
    out.push_back(static_cast<std::byte>((hexit(hex.at(i)) << 4U) | hexit(hex.at(i + 1))));
  }
  return out;
}

/// The fields of a line, apart by tabs.
std::vector<std::string> fields_of(const std::string& line) {
  std::vector<std::string> fields;
  std::istringstream in{line};
  std::string field;
  while (std::getline(in, field, '\t')) {
    fields.push_back(field);
  }
  return fields;
}

/// Reads the samples' file.
std::map<std::string, sample> read_samples() {
  std::ifstream in{std::string{TURBOREL_CPP_FIXTURES} + "/05-protocol-frames.tsv"};
  if (!in) {
    throw std::runtime_error{"the sample frames' fixture cannot be read"};
  }
  std::map<std::string, sample> read;
  std::string line;
  while (std::getline(in, line)) {
    const std::vector<std::string> fields = fields_of(line);
    if (line.starts_with('#') || fields.size() != 5) {
      continue;
    }
    read.emplace(fields.at(1), sample{
                                   .direction = fields.at(0),
                                   .name = fields.at(1),
                                   .needs = fields.at(2),
                                   .frame = unhex(fields.at(3)),
                                   .means = fields.at(4),
                               });
  }
  return read;
}

} // namespace

const std::map<std::string, sample>& samples() {
  static const std::map<std::string, sample> read = read_samples();
  return read;
}

const bytes& frame(const std::string& name) { return samples().at(name).frame; }

bytes joined(std::initializer_list<bytes> parts) {
  bytes out;
  for (const bytes& part : parts) {
    out.insert(out.end(), part.begin(), part.end());
  }
  return out;
}

writer& writer::u8(std::uint8_t value) {
  body_.push_back(static_cast<std::byte>(value));
  return *this;
}

writer& writer::u32(std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) {
    body_.push_back(static_cast<std::byte>(value >> (8U * i)));
  }
  return *this;
}

writer& writer::u64(std::uint64_t value) {
  for (unsigned i = 0; i < 8; ++i) {
    body_.push_back(static_cast<std::byte>(value >> (8U * i)));
  }
  return *this;
}

writer& writer::text(std::string_view value) { return raw(std::as_bytes(std::span{value})); }

writer& writer::raw(std::span<const std::byte> value) {
  u32(static_cast<std::uint32_t>(value.size()));
  return octets(value);
}

writer& writer::texts(std::initializer_list<std::string_view> values) {
  u32(static_cast<std::uint32_t>(values.size()));
  for (const std::string_view value : values) {
    text(value);
  }
  return *this;
}

writer& writer::octets(std::span<const std::byte> value) {
  body_.insert(body_.end(), value.begin(), value.end());
  return *this;
}

bytes writer::frame(char kind) const {
  writer header;
  header.u8(static_cast<std::uint8_t>(kind)).u32(static_cast<std::uint32_t>(body_.size()));
  bytes out = header.body_;
  out.insert(out.end(), body_.begin(), body_.end());
  return out;
}

bytes ready(std::initializer_list<std::string_view> capabilities) {
  const bytes key(16, std::byte{0xA5});
  return writer{}.text("ANN").octets(key).texts(capabilities).text("turborel 0.0.0 (alpha)").frame('R');
}

bytes complete() { return writer{}.u32(0).text("00000").u8(0).frame('K'); }

scripted::scripted(bytes server, bool bounds) : server_{std::move(server)}, bounds_{bounds} {}

std::optional<std::size_t> scripted::read(std::span<std::byte> into) {
  const std::size_t count = std::min(into.size(), server_.size() - at_);
  std::copy_n(server_.begin() + static_cast<std::ptrdiff_t>(at_), count, into.begin());
  at_ += count;
  return count;
}

std::optional<std::size_t> scripted::write(std::span<const std::byte> bytes) {
  written_.insert(written_.end(), bytes.begin(), bytes.end());
  return bytes.size();
}

void scripted::close() noexcept { closed_ = true; }

bool scripted::bound(std::chrono::milliseconds wait) noexcept {
  bound_ = wait;
  return true;
}

scripted_connection open_scripted(bytes server, const turborel::connect_options& options) {
  auto stream = std::make_unique<scripted>(std::move(server));
  scripted* kept = stream.get();
  turborel::mechanism trust = turborel::mechanism::trust();
  turborel::connection conn = turborel::connection::open(std::move(stream), "ANN", trust, options);
  return scripted_connection{.conn = std::move(conn), .stream = kept};
}

turborel::result native(const bytes& description, std::initializer_list<bytes> rows) {
  bytes all = description;
  for (const bytes& batch : rows) {
    all.insert(all.end(), batch.begin(), batch.end());
  }
  const bytes end = complete();
  all.insert(all.end(), end.begin(), end.end());
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the octets read as a stream's chars.
  std::istringstream in{std::string{reinterpret_cast<const char*>(all.data()), all.size()}};
  return turborel::read_native(in);
}

bytes octets_of(std::string_view text) {
  const auto view = std::as_bytes(std::span{text});
  return {view.begin(), view.end()};
}

std::optional<std::string> variable(const char* name) {
#ifdef _WIN32
  char* value = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
    return std::nullopt;
  }
  std::string copied{value};
  std::free(value); // NOLINT(cppcoreguidelines-no-malloc): _dupenv_s's copy, which it says to free.
  return copied;
#else
  // NOLINTNEXTLINE(concurrency-mt-unsafe): the tests that read one run on one thread.
  const char* value = std::getenv(name);
  return value == nullptr ? std::nullopt : std::optional<std::string>{value};
#endif
}

void set_variable(const char* name, const char* value) {
#ifdef _WIN32
  static_cast<void>(_putenv_s(name, value == nullptr ? "" : value));
#else
  // NOLINTNEXTLINE(concurrency-mt-unsafe): the tests that set one run on one thread.
  static_cast<void>(value == nullptr ? unsetenv(name) : setenv(name, value, 1));
#endif
}

bool has_zlib() {
  try {
    turborel::connect_options compressed;
    compressed.ask_none().ask(turborel::capability::compression);
    static_cast<void>(open_scripted(ready({"compression"}), compressed));
    return true;
  } catch (const turborel::error&) {
    return false;
  }
}

bool has_tls() {
  try {
    const turborel::tls probe;
    return true;
  } catch (const turborel::error&) {
    return false;
  }
}

} // namespace test
