/// @file
/// @brief Large objects through turborel-c [CLIENT Large objects]: ranges,
/// locators freed and held, readers and writers.

#include <turborel/lob.hpp>

#include "capi.hpp"
#include "connection_state.hpp"

#include <turborel/connection.hpp>
#include <turborel/params.hpp>
#include <turborel/turborel.h>
#include <turborel/value.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <streambuf>
#include <string_view>
#include <vector>

namespace turborel {

namespace {

/// The octets of `out` as turborel-c writes them.
std::uint8_t* c_octets(std::span<std::byte> out) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): std::byte and uint8_t are one byte alike, at C's boundary (R-CPP-4).
  return reinterpret_cast<std::uint8_t*>(out.data());
}

} // namespace

std::size_t read_lob(connection& conn, const value& lob, lob_range range, std::span<std::byte> out) {
  std::size_t length = out.size();
  const trl_lob_range c_range{.from = range.from, .count = range.count, .characters = range.characters};
  detail::check_on(conn, trl_lob_read(conn.get(), lob.c_value(), c_range, c_octets(out), &length));
  return length;
}

void free_lob(connection& conn, const value& lob) {
  detail::check_on(conn, trl_lob_free_locator(conn.get(), lob.c_value()));
}

void hold_lob(connection& conn, const value& lob) { detail::check_on(conn, trl_lob_hold(conn.get(), lob.c_value())); }

void lob_reader::freer::operator()(trl_lob_reader* made) const noexcept { trl_lob_reader_free(made); }

lob_reader::lob_reader(connection& conn, const value& lob) : conn_{&conn}, buffer_(lob_chunk_bytes) {
  trl_lob_reader* made = nullptr;
  detail::check_on(conn, trl_lob_reader_new(conn.get(), lob.c_value(), &made));
  reader_.reset(made);
}

std::size_t lob_reader::read(std::span<std::byte> out) {
  std::size_t length = 0;
  detail::check_on(*conn_, trl_lob_reader_read(reader_.get(), c_octets(out), out.size(), &length));
  return length;
}

std::vector<std::byte> lob_reader::read_all() {
  std::vector<std::byte> octets;
  std::vector<std::byte> chunk(lob_chunk_bytes);
  for (std::size_t read_now = read(chunk); read_now != 0; read_now = read(chunk)) {
    octets.insert(octets.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read_now));
  }
  return octets;
}

lob_reader::int_type lob_reader::underflow() {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the stream's chars are the object's octets.
  const std::size_t got = read({reinterpret_cast<std::byte*>(buffer_.data()), buffer_.size()});
  if (got == 0) {
    return traits_type::eof();
  }
  const std::span<char> filled{buffer_.data(), got};
  setg(filled.data(), filled.data(), std::to_address(filled.end()));
  return traits_type::to_int_type(filled.front());
}

void lob_writer::freer::operator()(trl_lob_writer* made) const noexcept { trl_lob_writer_free(made); }

lob_writer::lob_writer(connection& conn, bool character) : conn_{&conn} {
  trl_lob_writer* made = nullptr;
  detail::check_on(conn, trl_lob_writer_new(conn.get(), character, &made));
  writer_.reset(made);
}

lob_writer& lob_writer::write(std::span<const std::byte> bytes) {
  if (bytes.empty()) {
    return *this;
  }
  detail::check_on(*conn_, trl_lob_write(writer_.get(), detail::octets(bytes), bytes.size()));
  return *this;
}

lob_writer& lob_writer::write(std::string_view text) { return write(std::as_bytes(std::span{text})); }

void lob_writer::finish(params& values) {
  detail::check_on(*conn_, trl_lob_writer_finish(writer_.get(), values.get()));
}

} // namespace turborel
