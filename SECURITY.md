# Security Policy

Openstaller generates inspectable installation scripts. It is not a sandbox, a
privilege broker, or a package trust authority.

## Supported Security Properties

- deterministic payload identity
- visible install and uninstall scripts
- user-scoped registration by default
- license prompt support
- reproducible manifest metadata

## Not Yet Supported

- signed package manifests
- privileged machine-wide installation brokers
- per-file uninstall rollback journals
- payload archive verification
- remote update channels

## Reporting Issues

Please report security-sensitive issues privately until a fix is prepared. For
public bug reports, include the generated `manifest.openstaller`, the command
used to generate the package, and the affected platform.
