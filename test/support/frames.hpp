/// @file
/// @brief What the tests that read frames through the driver share
/// (R-TEST-7): the sample frames of Turborel's 05-protocol-frames.tsv, a
/// writer of frames for what the samples lack, a stream the tests script,
/// and connections over it.

#ifndef TURBOREL_TEST_SUPPORT_FRAMES_HPP
#define TURBOREL_TEST_SUPPORT_FRAMES_HPP

#include <turborel/connection.hpp>
#include <turborel/result.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace test {

/// Bytes, as frames are written.
using bytes = std::vector<std::byte>;

/// A line of the sample frames' file [PROTOCOL Sample frames].
struct sample {
  /// `client` or `server`.
  std::string direction;
  /// The sample's name.
  std::string name;
  /// The capabilities it needs, or `-`.
  std::string needs;
  /// The whole frame.
  bytes frame;
  /// What it decodes to, in words.
  std::string means;
};

/// Every sample of test/fixtures/05-protocol-frames.tsv, by its name, read
/// once.
/// @return The samples.
const std::map<std::string, sample>& samples();

/// A sample's frame, by its name.
/// @param name The sample's name.
/// @return Its frame.
const bytes& frame(const std::string& name);

/// Frames joined, in order.
/// @param parts The frames.
/// @return The bytes.
bytes joined(std::initializer_list<bytes> parts);

/// Writes a frame's body field by field, as 05-protocol.md lays each out.
class writer {
public:
  /// A `u8`.
  writer& u8(std::uint8_t value);
  /// A `u16`, as the low half of a `u32` is not; used for nothing but i16.
  writer& u32(std::uint32_t value);
  /// A `u64`.
  writer& u64(std::uint64_t value);
  /// A string: its length, `u32`, then its UTF-8.
  writer& text(std::string_view value);
  /// Bytes: their count, `u32`, then them.
  writer& raw(std::span<const std::byte> value);
  /// Strings: a count, then each.
  writer& texts(std::initializer_list<std::string_view> values);
  /// Octets as they are, with no count.
  writer& octets(std::span<const std::byte> value);
  /// The frame of `kind` and this body.
  [[nodiscard]] bytes frame(char kind) const;

private:
  bytes body_;
};

/// A Ready granting `capabilities`, authorization ANN, a key of 16 A5s.
/// @param capabilities The capabilities granted.
/// @return The frame.
bytes ready(std::initializer_list<std::string_view> capabilities);

/// A Complete of SQLCODE 0, 00000, no transaction active, no tail.
/// @return The frame.
bytes complete();

/// A stream the test scripts: it reads the server's bytes it was given,
/// then its end, and keeps what the client writes.
class scripted : public turborel::byte_stream {
public:
  /// A stream that reads `server`, then its end.
  /// @param server What the server sends.
  /// @param bounds Whether it bounds its reads and writes.
  explicit scripted(bytes server, bool bounds = false);

  std::optional<std::size_t> read(std::span<std::byte> into) override;
  std::optional<std::size_t> write(std::span<const std::byte> bytes) override;
  void close() noexcept override;
  [[nodiscard]] bool bounds() const noexcept override { return bounds_; }
  bool bound(std::chrono::milliseconds wait) noexcept override;

  /// What the client wrote.
  /// @return The bytes.
  [[nodiscard]] const test::bytes& written() const noexcept { return written_; }
  /// Whether the connection closed it.
  /// @return Whether.
  [[nodiscard]] bool closed() const noexcept { return closed_; }
  /// The last bound the connection set.
  /// @return The bound.
  [[nodiscard]] std::chrono::milliseconds last_bound() const noexcept { return bound_; }

private:
  test::bytes server_;
  std::size_t at_{};
  test::bytes written_;
  bool bounds_;
  bool closed_{};
  std::chrono::milliseconds bound_{};
};

/// What a scripted connection keeps for the test: the connection, and its
/// stream, which the connection owns.
struct scripted_connection {
  /// The connection.
  turborel::connection conn;
  /// Its stream.
  scripted* stream;
};

/// A connection by trust over a stream that reads `server`, the login's
/// replies first.
/// @param server What the server sends.
/// @param options What to ask for.
/// @return The connection and its stream.
scripted_connection open_scripted(bytes server, const turborel::connect_options& options = turborel::connect_options{});

/// The result of native rows: a Description, the frames, and a Complete,
/// read back as an export's are.
/// @param description The Description.
/// @param rows The Rows frames.
/// @return The result.
turborel::result native(const bytes& description, std::initializer_list<bytes> rows);

/// The bytes of `text`.
/// @param text The text.
/// @return Its octets.
bytes octets_of(std::string_view text);

/// The process's environment variable `name`, as the platform reads it.
/// @param name The variable.
/// @return Its value; none if it is not set.
std::optional<std::string> variable(const char* name);

/// Sets the process's environment variable `name` to `value`, or unsets
/// it of none, as the platform does; the tests that do run on one thread.
/// @param name The variable.
/// @param value Its value, or null.
void set_variable(const char* name, const char* value);

/// Whether turborel-c was built with a TLS library, whose hashes SCRAM
/// needs too; of one built with none, the TLS and SCRAM tests are skipped.
/// @return Whether it was.
bool has_tls();

/// Whether turborel-c was built with zlib, which compressed frames need;
/// of one built with none, the tests of compression are skipped.
/// @return Whether it was.
bool has_zlib();

} // namespace test

/// Skips a test of compressed frames where turborel-c was built with no
/// zlib, as where the platform has none.
#define ZLIB_OR_SKIP()                                                                                                 \
  do {                                                                                                                 \
    if (!test::has_zlib()) {                                                                                           \
      SKIP("turborel-c was built with no zlib, so compressed frames are not had");                                     \
    }                                                                                                                  \
  } while (false)

/// Skips a test of TLS or SCRAM where turborel-c was built with no TLS
/// library, as of an OpenSSL older than it takes.
#define TLS_OR_SKIP()                                                                                                  \
  do {                                                                                                                 \
    if (!test::has_tls()) {                                                                                            \
      SKIP("turborel-c was built with no TLS library, so TLS and SCRAM are not had");                                  \
    }                                                                                                                  \
  } while (false)

#endif // TURBOREL_TEST_SUPPORT_FRAMES_HPP
