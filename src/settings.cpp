/// @file
/// @brief Connection settings through turborel-c [CLIENT Connecting by
/// settings].

#include <turborel/settings.hpp>

#include "capi.hpp"

#include <turborel/connect.hpp>
#include <turborel/connection.hpp>
#include <turborel/turborel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

namespace turborel {

namespace {

/// The program's reader of the environment, as turborel-c calls it: what
/// it gave kept until the next call, and what it threw.
struct environment_of {
  const settings::variables* read;
  std::optional<std::string> last;
  std::exception_ptr thrown;
};

/// Reads a variable through the program's reader (R-RAII-2); one that
/// threw reads as unset, and its exception is thrown once settings are
/// laid.
const char* variable(void* context, const char* name) noexcept {
  auto& held = *static_cast<environment_of*>(context);
  try {
    held.last = (*held.read)(detail::view(name));
  } catch (...) {
    held.last.reset();
    if (!held.thrown) {
      held.thrown = std::current_exception();
    }
  }
  return held.last ? held.last->c_str() : nullptr;
}

} // namespace

void options::freer::operator()(trl_options* made) const noexcept { trl_options_free(made); }

options::options() {
  trl_options* made = nullptr;
  detail::check(trl_options_new(&made));
  options_.reset(made);
}

options options::from_uri(const std::string& uri) {
  trl_options* made = nullptr;
  const trl_status status = trl_options_from_uri(uri.c_str(), &made);
  options layer{made};
  detail::check(status, trl_options_failure(layer.c_options()));
  return layer;
}

options& options::set(std::string_view key, std::string_view value) {
  const trl_status status = trl_options_set(options_.get(), std::string{key}.c_str(), std::string{value}.c_str());
  detail::check(status, trl_options_failure(c_options()));
  return *this;
}

std::optional<std::string_view> options::get(std::string_view key) const {
  return detail::maybe(trl_options_get(c_options(), std::string{key}.c_str()));
}

void settings::freer::operator()(trl_settings* made) const noexcept { trl_settings_free(made); }

settings settings::resolve(const options& layer) {
  trl_settings* made = nullptr;
  const trl_status status = trl_settings_resolve(layer.c_options(), &made);
  settings laid{made};
  detail::check(status, trl_settings_failure(laid.get()));
  return laid;
}

settings settings::resolve(const options& layer, const variables& environment) {
  environment_of held{.read = &environment, .last = std::nullopt, .thrown = nullptr};
  trl_settings* made = nullptr;
  const trl_status status = trl_settings_resolve_in(layer.c_options(), variable, &held, &made);
  settings laid{made};
  if (held.thrown) {
    std::rethrow_exception(held.thrown);
  }
  detail::check(status, trl_settings_failure(laid.get()));
  return laid;
}

settings settings::from_uri(const std::string& uri) {
  trl_settings* made = nullptr;
  const trl_status status = trl_settings_from_uri(uri.c_str(), &made);
  settings laid{made};
  detail::check(status, trl_settings_failure(laid.get()));
  return laid;
}

std::size_t settings::host_count() const noexcept { return trl_settings_host_count(get()); }

server_address settings::host(std::size_t index) const {
  std::uint16_t port = 0;
  bool local = false;
  const char* name = trl_settings_host(get(), index, &port, &local);
  if (name == nullptr) {
    detail::raise_misuse("the settings name no server " + std::to_string(index));
  }
  return server_address{.name = name, .port = port, .local = local};
}

std::string_view settings::user() const noexcept { return detail::view(trl_settings_user(get())); }

std::optional<std::string_view> settings::get(std::string_view key) const {
  return detail::maybe(trl_settings_get(get(), std::string{key}.c_str()));
}

turborel::tls_mode settings::tls_mode() const noexcept {
  switch (trl_settings_tls_mode(get())) {
  case TRL_TLS_DISABLE:
    return tls_mode::disable;
  case TRL_TLS_REQUIRE:
    return tls_mode::require;
  case TRL_TLS_VERIFY_CA:
    return tls_mode::verify_ca;
  case TRL_TLS_VERIFY_FULL:
    return tls_mode::verify_full;
  case TRL_TLS_PREFER:
    break;
  }
  return tls_mode::prefer;
}

turborel::channel_binding settings::channel_binding() const noexcept {
  switch (trl_settings_channel_binding(get())) {
  case TRL_CHANNEL_BINDING_DISABLE:
    return channel_binding::disable;
  case TRL_CHANNEL_BINDING_REQUIRE:
    return channel_binding::require;
  case TRL_CHANNEL_BINDING_PREFER:
    break;
  }
  return channel_binding::prefer;
}

std::chrono::milliseconds settings::connect_timeout() const noexcept {
  return std::chrono::milliseconds{trl_settings_connect_timeout(get())};
}

target_role settings::target() const noexcept {
  switch (trl_settings_target(get())) {
  case TRL_TARGET_READ_WRITE:
    return target_role::read_write;
  case TRL_TARGET_READ_ONLY:
    return target_role::read_only;
  case TRL_TARGET_ANY:
    break;
  }
  return target_role::any;
}

turborel::load_balance settings::load_balance() const noexcept {
  return trl_settings_load_balance(get()) == TRL_LOAD_BALANCE_RANDOM ? load_balance::random : load_balance::disable;
}

bool settings::tls_system_roots() const noexcept { return trl_settings_tls_system_roots(get()); }

std::optional<std::string> settings::password_for(std::size_t host) const {
  std::size_t length = 0;
  if (!trl_settings_password_for(get(), host, nullptr, 0, &length)) {
    return std::nullopt;
  }
  std::string password(length + 1, '\0');
  static_cast<void>(trl_settings_password_for(get(), host, password.data(), password.size(), &length));
  password.resize(length);
  return password;
}

key_cache::lookups settings::key_cache_lookups() const noexcept {
  key_cache::lookups counted;
  trl_key_cache_lookups(trl_settings_key_cache(get()), &counted.hits, &counted.misses);
  return counted;
}

connection settings::connect() const {
  trl_conn* made = nullptr;
  const trl_status status = trl_settings_connect(get(), &made);
  connection opened{made};
  detail::check(status, trl_conn_failure(opened.get()));
  return opened;
}

} // namespace turborel
