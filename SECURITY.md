# Security Policy

## Supported versions

Security fixes target the latest `main` and the most recent tagged release.

## Reporting a vulnerability

Please **do not** open a public issue for a security vulnerability. Instead,
report it privately to the maintainers (open a private security advisory on the
GitHub repo, or email the maintainers listed in the org profile). Include:

- a description of the issue and its impact,
- steps / input to reproduce (a malformed `.flux`/`.fluxa`/USDA/MAVLink file),
- the build platform and version.

We aim to acknowledge within 72 hours and to ship a fix in the next patch
release, crediting you unless you prefer otherwise.

## Trust model

FLUXmeme treats **all input as untrusted**: `.flux`, `.fluxa`, USDA, URDF/SDF,
and MAVLink frames are parsed with size/count/depth caps (see SPEC §8) to bound
resource use. The codec layer performs **no arbitrary execution** — signal/param
records carry only numeric values; USD/OKF/A2A carry only data. A parser bug that
causes a crash or excessive resource use on crafted input is treated as a
security issue.
