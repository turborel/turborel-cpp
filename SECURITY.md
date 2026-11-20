# Security

Report a vulnerability privately, never in a public issue: on the
repository's **Security** tab, choose **Report a vulnerability**. Only the
maintainers see the report.

What counts, for a driver:

- TLS not used, or not verified, where it was asked for, as by a
  `tls_mode` of `require`, `verify-ca` or `verify-full`, or a
  `channel_binding` of `require`;
- a password, a SCRAM key or anything else derived from a password, or a
  session's cancel key, disclosed: sent where it should not be, as over
  plain TCP, kept where another can read it, or written to a log or an
  error;
- a server's reply, or bytes on the wire, that can crash the client, run
  code in it, or make it take memory without bound.

A wrong value, a leak with no attacker, or a crash only the program's own
misuse of the API can cause is a bug: report it with the **Bug** form, its
impact in the title (CONTRIBUTING.md, Reporting issues). A flaw of the
protocol's handling that lies in turborel-c is reported to turborel-c; one
of the server, to turborel.

Say in the report:

- what an attacker needs, such as a network position between client and
  server, a server of their own, or a user;
- what they gain;
- the commit you tested (`git rev-parse --short=12 HEAD`);
- the smallest steps that show it.

You will hear back within a week. The fix is made in private and
published with an advisory that credits you, unless you ask not to be
named.
