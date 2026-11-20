/// @file
/// @brief Targets, TLS, mechanisms, the SCRAM key cache and connection
/// options, made through turborel-c [CLIENT Connecting].

#include <turborel/connect.hpp>

#include "capi.hpp"

#include <turborel/error.hpp>

#include <turborel/turborel.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace turborel {

// The driver's constants are turborel-c's, which its calls use.
static_assert(connect_within == std::chrono::milliseconds{TRL_CONNECT_WITHIN_MS});
static_assert(terminate_within == std::chrono::milliseconds{TRL_TERMINATE_WITHIN_MS});
static_assert(cancel_within == std::chrono::milliseconds{TRL_CANCEL_WITHIN_MS});
static_assert(max_body_bytes == TRL_MAX_BODY_BYTES);
static_assert(default_port == TRL_DEFAULT_PORT);

namespace {

/// Checks a call that keeps no failure of a handle.
void made(trl_status status) { detail::check(status); }

/// A timeout in turborel-c's milliseconds.
std::uint32_t milliseconds(std::chrono::milliseconds wait) {
  if (wait.count() < 0 || wait.count() > std::numeric_limits<std::uint32_t>::max()) {
    detail::raise_misuse("a timeout of " + std::to_string(wait.count()) + " ms is not from 0 to 2^32 - 1");
  }
  return static_cast<std::uint32_t>(wait.count());
}

} // namespace

void tls::freer::operator()(trl_tls* tls) const noexcept { trl_tls_free(tls); }

tls::tls() {
  trl_tls* settings = nullptr;
  const trl_status status = trl_tls_new(&settings);
  tls_.reset(settings);
  detail::check(status, trl_tls_failure(get()));
}

tls& tls::trust_file(const std::string& path) {
  detail::check(trl_tls_trust_file(get(), path.c_str()), trl_tls_failure(get()));
  return *this;
}

tls& tls::trust_platform() {
  detail::check(trl_tls_trust_platform(get()), trl_tls_failure(get()));
  return *this;
}

tls& tls::present(const std::string& chain, const std::string& key) {
  detail::check(trl_tls_present(get(), chain.c_str(), key.c_str()), trl_tls_failure(get()));
  return *this;
}

void target::freer::operator()(trl_target* target) const noexcept { trl_target_free(target); }

target target::tcp(const std::string& host, std::uint16_t port) {
  trl_target* made_target = nullptr;
  made(trl_target_tcp(host.c_str(), port, &made_target));
  return target{made_target};
}

target target::local(const std::string& path) {
  trl_target* made_target = nullptr;
  made(trl_target_local(path.c_str(), &made_target));
  return target{made_target};
}

target target::over_tls(const std::string& host, std::uint16_t port, const tls& settings,
                        const std::optional<std::string>& server_name) {
  trl_target* made_target = nullptr;
  const char* name = server_name ? server_name->c_str() : nullptr;
  const trl_status status = trl_target_tls(host.c_str(), port, name, settings.get(), &made_target);
  target held{made_target};
  detail::check(status, trl_target_failure(held.get()));
  return held;
}

bool target::is_plain_tcp() const noexcept { return trl_target_is_plain_tcp(get()); }

bool target::is_tls() const noexcept { return trl_target_is_tls(get()); }

std::optional<std::string_view> target::host() const noexcept { return detail::maybe(trl_target_host(get())); }

key_cache::key_cache() {
  trl_key_cache* cache = nullptr;
  made(trl_key_cache_new(&cache));
  cache_ = std::shared_ptr<trl_key_cache>{cache, [](trl_key_cache* held) { trl_key_cache_free(held); }};
}

std::size_t key_cache::size() const noexcept { return trl_key_cache_size(get()); }

key_cache::lookups key_cache::counts() const noexcept {
  lookups counted;
  trl_key_cache_lookups(get(), &counted.hits, &counted.misses);
  return counted;
}

void key_cache::clear() noexcept { trl_key_cache_clear(get()); }

void mechanism::freer::operator()(trl_mechanism* mechanism) const noexcept { trl_mechanism_free(mechanism); }

mechanism mechanism::trust() {
  trl_mechanism* made_mechanism = nullptr;
  made(trl_mechanism_trust(&made_mechanism));
  return mechanism{made_mechanism};
}

mechanism mechanism::password(const std::string& secret) {
  trl_mechanism* made_mechanism = nullptr;
  made(trl_mechanism_password(secret.c_str(), &made_mechanism));
  return mechanism{made_mechanism};
}

mechanism mechanism::certificate() {
  trl_mechanism* made_mechanism = nullptr;
  made(trl_mechanism_certificate(&made_mechanism));
  return mechanism{made_mechanism};
}

mechanism mechanism::scram(const std::string& secret) {
  trl_mechanism* made_mechanism = nullptr;
  const trl_status status = trl_mechanism_scram(secret.c_str(), &made_mechanism);
  mechanism held{made_mechanism};
  detail::check(status, trl_mechanism_failure(held.get()));
  return held;
}

mechanism& mechanism::in_the_clear() {
  made(trl_mechanism_scram_in_the_clear(get()));
  return *this;
}

mechanism& mechanism::bind_channel(channel_binding how) {
  trl_channel_binding binding = TRL_CHANNEL_BINDING_PREFER;
  switch (how) {
  case channel_binding::require:
    binding = TRL_CHANNEL_BINDING_REQUIRE;
    break;
  case channel_binding::disable:
    binding = TRL_CHANNEL_BINDING_DISABLE;
    break;
  case channel_binding::prefer:
    break;
  }
  made(trl_mechanism_scram_channel_binding(get(), binding));
  return *this;
}

mechanism& mechanism::with_cache(const key_cache& cache) {
  made(trl_mechanism_scram_key_cache(get(), cache.get()));
  return *this;
}

mechanism& mechanism::bind(std::span<const std::byte> certificate, std::span<const std::byte> exporter) {
  const std::uint8_t* der = certificate.empty() ? nullptr : detail::octets(certificate);
  const std::uint8_t* keys = exporter.empty() ? nullptr : detail::octets(exporter);
  made(trl_mechanism_bind(get(), der, certificate.size(), keys, exporter.size()));
  return *this;
}

std::string_view mechanism::name() const noexcept { return detail::view(trl_mechanism_name(get())); }

void connect_options::freer::operator()(trl_conn_options* options) const noexcept { trl_conn_options_free(options); }

connect_options::connect_options() {
  trl_conn_options* options = nullptr;
  made(trl_conn_options_new(&options));
  options_.reset(options);
}

connect_options& connect_options::ask_none() {
  made(trl_conn_options_ask_none(options_.get()));
  return *this;
}

connect_options& connect_options::ask(std::string_view name) {
  made(trl_conn_options_ask(options_.get(), std::string{name}.c_str()));
  return *this;
}

connect_options& connect_options::attribute(std::string_view name, std::string_view value) {
  const trl_status status =
      trl_conn_options_attribute(options_.get(), std::string{name}.c_str(), std::string{value}.c_str());
  if (status == TRL_INVALID_ARGUMENT) {
    // The drivers' shared table: refused before sending, as the server
    // would fail the connection [PROTOCOL The tail].
    throw error{failure{
        .sqlcode = detail::broken_sqlcode,
        .sqlstate = std::string{detail::broken_sqlstate},
        .message = "a connection attribute is past the protocol's bounds: 32 attributes, a name of "
                   "1 to 64 bytes, a value of at most 1024",
    }};
  }
  made(status);
  return *this;
}

connect_options& connect_options::compress_from(std::size_t bytes) {
  made(trl_conn_options_compress_from(options_.get(), bytes));
  return *this;
}

connect_options& connect_options::timeouts(std::chrono::milliseconds connect, std::chrono::milliseconds read) {
  made(trl_conn_options_timeouts(options_.get(), milliseconds(connect), milliseconds(read)));
  return *this;
}

} // namespace turborel
