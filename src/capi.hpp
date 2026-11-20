/// @file
/// @brief What every source that calls turborel-c shares (RULES.md D1,
/// D10): owners of its handles, the check that turns a call's status into
/// the driver's error, and the conversions at the boundary between C's
/// pointers and C++'s spans and views.

#ifndef TURBOREL_SRC_CAPI_HPP
#define TURBOREL_SRC_CAPI_HPP

#include <turborel/error.hpp>
#include <turborel/turborel.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace turborel::detail {

/// Frees a handle of turborel-c by its `_free` function, which takes null.
/// @tparam Free The function.
template <auto Free>
struct freer {
  /// Frees `handle`.
  /// @param handle The handle, or null.
  template <typename T>
  void operator()(T* handle) const noexcept {
    Free(handle);
  }
};

/// The owner of a handle of turborel-c, freed by `Free` (R-RAII-1).
template <typename T, auto Free>
using owner = std::unique_ptr<T, freer<Free>>;

/// SQLCODE -910 and SQLSTATE TB910: a misuse of the API turborel-client
/// has no case for (the drivers' shared table of failures).
inline constexpr std::int32_t misuse_sqlcode = -910;
/// See misuse_sqlcode.
inline constexpr std::string_view misuse_sqlstate = "TB910";
/// SQLCODE -919: a connection failure [PROTOCOL Errors and notices].
inline constexpr std::int32_t broken_sqlcode = -919;
/// SQLSTATE 08006, connection failure.
inline constexpr std::string_view broken_sqlstate = "08006";
/// SQLCODE -301: a data exception, a value that cannot be carried or read.
inline constexpr std::int32_t data_sqlcode = -301;
/// SQLSTATE 22018, text that writes no value of the type asked for.
inline constexpr std::string_view invalid_text_sqlstate = "22018";
/// SQLSTATE 22003, a value out of the range of the type asked for.
inline constexpr std::string_view out_of_range_sqlstate = "22003";

/// The driver's failure of a failure turborel-c keeps: every field copied,
/// so it outlives the handle.
/// @param failure The failure, not null.
/// @return The failure.
[[nodiscard]] failure failure_of(const trl_failure* failure);

/// Throws the driver's error of a status other than TRL_OK (R-ERR-1):
/// TRL_FAILED as the failure the handle keeps; TRL_NO_MEMORY as
/// std::bad_alloc; the rest as the misuse or data exception they are, as
/// is TRL_FAILED of a handle that keeps no failure.
/// @param status The status.
/// @param kept The handle's failure, read after the call returned, or null.
/// @throws turborel::error Of every status but TRL_OK and TRL_NO_MEMORY.
/// @throws std::bad_alloc Of TRL_NO_MEMORY.
[[noreturn]] void raise(trl_status status, const trl_failure* kept);

/// Checks a call's status, reading the failure the handle keeps only if
/// it failed. A handle's failure is at an address that lasts as long as
/// the handle, so `kept` may be had before the call returns; what it holds
/// is read only after.
/// @param status The status.
/// @param kept The handle's failure, or null of a handle not made.
/// @throws turborel::error As raise() does.
inline void check(trl_status status, const trl_failure* kept) {
  if (status != TRL_OK) {
    raise(status, kept);
  }
}

/// Checks the status of a call of no handle, which keeps no failure.
/// @param status The status.
/// @throws turborel::error As raise() does.
inline void check(trl_status status) {
  if (status != TRL_OK) {
    raise(status, nullptr);
  }
}

/// Throws the misuse failure -910/TB910 with `message`.
/// @param message What was misused.
/// @throws turborel::error Always.
[[noreturn]] void raise_misuse(std::string message);

/// The bytes of a byte span as turborel-c takes them.
/// @param bytes The bytes.
/// @return Their address, as octets.
[[nodiscard]] inline const std::uint8_t* octets(std::span<const std::byte> bytes) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): std::byte and uint8_t are one byte alike, at C's boundary (R-CPP-4).
  return reinterpret_cast<const std::uint8_t*>(bytes.data());
}

/// The bytes turborel-c gives as a byte span.
/// @param data Their address, or null of none.
/// @param length Their count.
/// @return The span.
[[nodiscard]] inline std::span<const std::byte> byte_span(const std::uint8_t* data, std::size_t length) noexcept {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): std::byte and uint8_t are one byte alike, at C's boundary (R-CPP-4).
  return {reinterpret_cast<const std::byte*>(data), length};
}

/// A string turborel-c gives, as a view; null as empty.
/// @param text The string, or null.
/// @return The view.
[[nodiscard]] inline std::string_view view(const char* text) noexcept {
  return text == nullptr ? std::string_view{} : std::string_view{text};
}

/// A string turborel-c gives that may be null, as an optional view.
/// @param text The string, or null.
/// @return The view, or none.
[[nodiscard]] inline std::optional<std::string_view> maybe(const char* text) noexcept {
  return text == nullptr ? std::nullopt : std::optional<std::string_view>{text};
}

/// Text turborel-c writes into a buffer the caller gives, as
/// trl_value_to_text() does: asked once for its length, then written.
/// @param write Writes into a buffer of a capacity, giving the whole length.
/// @return The text.
template <typename Write>
[[nodiscard]] std::string written(const Write& write) {
  std::size_t length = 0;
  write(nullptr, 0, &length);
  std::string text(length + 1, '\0');
  write(text.data(), text.size(), &length);
  text.resize(length);
  return text;
}

} // namespace turborel::detail

#endif // TURBOREL_SRC_CAPI_HPP
