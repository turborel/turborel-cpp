# Appendix C: Settings

[Contents](README.md)

Every connection setting: its name, the same in `turborel::options`, a
URI's query and a service file, and among `turborel::setting`'s
constants; its variable of the environment, if it has one; what it says;
and its default [CLIENT Where settings come from]. Each is taken from the
first layer that has it: the program's options or URI, the service's
section, the environment, the defaults. A variable set but empty gives
nothing.

| Setting | Variable | What it says | Default |
|---|---|---|---|
| `host` | TURBOREL_HOST | The servers, by name or address, comma-separated; each may carry its port as `name:port`, an IPv6 address as `[address]:port`; a path names a Unix socket (on Windows, a named pipe) | `localhost` |
| `port` | TURBOREL_PORT | The port of each host that carries none: one for all, or one each, comma-separated | 7422 |
| `user` | TURBOREL_USER | The user to connect as | none: it must be given |
| `password` | TURBOREL_PASSWORD | The user's password | the password file's |
| `tls_mode` | TURBOREL_TLS_MODE | `disable`, `prefer`, `require`, `verify-ca` or `verify-full` | `prefer` |
| `channel_binding` | | `disable`, `prefer` or `require` | `prefer` |
| `application_name` | TURBOREL_APPLICATION_NAME | The program's name, sent as the connection attribute `application` | none |
| `connect_timeout` | TURBOREL_CONNECT_TIMEOUT | Whole seconds each step of connecting waits; 0 for no limit | 30 |
| `service` | TURBOREL_SERVICE | The section of the service file to read | none |
| `service_file` | TURBOREL_SERVICE_FILE | The service file | `~/.turborel_service.conf` |
| `passfile` | TURBOREL_PASSFILE | The password file | `~/.turborel_pass` |
| `target` | | `any`, `read-write` or `read-only` | `any` |
| `load_balance` | | `disable`, the hosts tried in order, or `random` | `disable` |
| `tls_roots` | | A PEM file of the certificate authorities trusted | none |
| `tls_system_roots` | | `true` to trust the platform's authorities too | `false` |
| `tls_certificate`, `tls_key` | | PEM files of the client's certificate chain and key, to present | none |
| `server_name` | | The name the server's certificate is checked against | the host's |

A service file cannot set `service` or `service_file`. `~` is the home
directory: HOME, or on Windows USERPROFILE. A setting the table has not,
a word a setting does not take, or no user, is refused, SQLCODE -919,
SQLSTATE 08001.
