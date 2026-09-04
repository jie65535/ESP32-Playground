# Security Policy

## Scope

This policy covers the PGOS firmware and the host-side tools in this repository.

## Known limitations

- The TCP control, throughput, and mirror channels (19000-19002) currently do
  not provide TLS, authentication, or pairing. Keep them on a trusted LAN and
  do not forward these ports from a router.
- Wi-Fi credentials are intended to live in device NVS or an ignored local
  configuration file. Never commit real SSIDs, passwords, tokens, captures, or
  logs.
- A device running experimental firmware may expose hardware, audio, and network
  behavior that is not suitable for unattended or safety-critical use.

## Reporting a vulnerability

Please avoid posting credentials or an undisclosed exploit in a public issue.
Once the repository is published, use a private GitHub Security Advisory or
contact [@jie65535](https://github.com/jie65535) with the affected commit,
reproduction steps, and impact. Reports without sensitive details may use a
normal issue when no private disclosure is needed.

If a secret was ever committed, revoke or rotate it first. Removing a file in a
later commit does not remove it from Git history; history rewriting is required
before a public push.
