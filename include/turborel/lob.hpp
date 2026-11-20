/// @file
/// @brief Large objects [CLIENT Large objects]: a CLOB or BLOB read whole
/// from its row, or by its locator, a range or a chunk at a time, as a
/// std::streambuf; a locator freed or held; a new one written a piece at a
/// time and passed as a parameter by its locator (RULES.md D2).

#ifndef TURBOREL_LOB_HPP
#define TURBOREL_LOB_HPP

#include <turborel/connection.hpp>
#include <turborel/export.hpp>
#include <turborel/params.hpp>
#include <turborel/value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <streambuf>
#include <string_view>
#include <vector>

struct trl_lob_reader;
struct trl_lob_writer;

namespace turborel {

/// The most octets one read of a large object gives, a chunk.
inline constexpr std::size_t lob_chunk_bytes = 262144;
/// The longest large object a server sends whole by default,
/// SERVER.LOB_INLINE_BYTES; a longer one comes by its locator.
inline constexpr std::size_t lob_inline_bytes = 65536;

/// A range of a large object: where it begins, and how much.
struct lob_range {
  /// The first octet, or character, from 0.
  std::uint64_t from{};
  /// The most octets, or characters.
  std::uint64_t count{};
  /// Whether `from` and `count` are a CLOB's characters, whole ones.
  bool characters{};
};

/// Reads a range of a large object: of one whole, from the value; of one by
/// its locator, from the server, at most a chunk [PROTOCOL Lob 'L'].
/// @param conn The connection the value came over.
/// @param lob The large object value.
/// @param range The range.
/// @param out Where the octets go; a range in characters reads at most a
///   quarter as many characters as `out` holds octets.
/// @return How many octets were read.
/// @throws turborel::error Of a value of another kind (-910), or a locator
///   no longer valid (0F001).
std::size_t read_lob(connection& conn, const value& lob, lob_range range, std::span<std::byte> out);

/// Frees a large object's locator.
/// @param conn The connection.
/// @param lob A large object value that came by its locator.
/// @throws turborel::error Of one sent whole (22000), or not valid (0F001).
void free_lob(connection& conn, const value& lob);

/// Holds a large object's locator past its transaction's commit, until it
/// is freed or a transaction is rolled back.
/// @param conn The connection.
/// @param lob A large object value that came by its locator.
/// @throws turborel::error As free_lob() does.
void hold_lob(connection& conn, const value& lob);

/// A large object read from its beginning to its end, a chunk at a time,
/// as a std::streambuf, so a std::istream reads it [CLIENT Large objects].
class TURBOREL_CPP_EXPORT lob_reader : public std::streambuf {
public:
  /// A reader of `lob`, which uses `conn` until it is destroyed.
  /// @param conn The connection the value came over.
  /// @param lob The large object value, copied.
  /// @throws turborel::error Of a value of another kind (-910).
  lob_reader(connection& conn, const value& lob);

  /// Reads the next octets.
  /// @param out Where they go.
  /// @return How many, 0 at the end.
  /// @throws turborel::error Of the connection's failure, as 22000 of a
  ///   server that sent less than the value's length.
  std::size_t read(std::span<std::byte> out);

  /// Reads every octet left.
  /// @return The octets.
  /// @throws turborel::error As read() does.
  [[nodiscard]] std::vector<std::byte> read_all();

protected:
  /// Fills the stream's buffer with the next chunk.
  /// @return The next character, or the end.
  int_type underflow() override;

private:
  /// Frees a reader.
  struct freer {
    /// Frees `made`.
    /// @param made The reader.
    void operator()(trl_lob_reader* made) const noexcept;
  };

  connection* conn_;
  std::unique_ptr<trl_lob_reader, freer> reader_;
  std::vector<char> buffer_;
};

/// A new large object, written a piece at a time [CLIENT Large objects]:
/// sent in Appends of a chunk as it comes, a CLOB's octets UTF-8, a
/// character free to be split between writes.
class TURBOREL_CPP_EXPORT lob_writer {
public:
  /// Creates an empty large object on the server, to write.
  /// @param conn The connection, which the writer uses until destroyed.
  /// @param character Whether it is a CLOB, else a BLOB.
  /// @throws turborel::error The connection's failure.
  lob_writer(connection& conn, bool character);

  /// Writes octets.
  /// @param bytes The octets.
  /// @return The writer.
  /// @throws turborel::error Of a CLOB whose octets are not UTF-8 (22021).
  lob_writer& write(std::span<const std::byte> bytes);

  /// Writes text, a CLOB's.
  /// @param text The text.
  /// @return The writer.
  /// @throws turborel::error As write() does.
  lob_writer& write(std::string_view text);

  /// Sends what is left, and adds the large object to `values` as its next
  /// value, by its locator, valid until its transaction ends.
  /// @param values The parameters.
  /// @throws turborel::error Of a CLOB whose last octets are not UTF-8.
  void finish(params& values);

private:
  /// Frees a writer.
  struct freer {
    /// Frees `made`.
    /// @param made The writer.
    void operator()(trl_lob_writer* made) const noexcept;
  };

  connection* conn_;
  std::unique_ptr<trl_lob_writer, freer> writer_;
};

} // namespace turborel

#endif // TURBOREL_LOB_HPP
