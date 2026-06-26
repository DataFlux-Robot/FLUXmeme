# Contributing to FLUXmeme

Thanks for your interest in FLUXmeme — the self-describing asset format for
embodied nodes. This is a reference implementation of the [spec](SPEC.md);
contributions that improve correctness, portability, or clarity are welcome.

## Build & test

```bat
:: Windows (VS 2022 Developer prompt, or the VS-bundled CMake):
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\bin\Release\demo_okf.exe   :: and the other demos / tests
```

```bash
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

All demos and tests must print `PASS` / exit 0 before a PR. Keep the suite
green; a change that regresses a round-trip demo is not ready.

## Code style

- C11, MSVC-safe (no GCC extensions, no VLAs, no nested functions).
- `clang-format` (`.clang-format`) is the source of truth; format before commit.
- Storage layer stays **dumb**: only TEXT/BIN blobs + structured metadata. All
  parsing/rendering/solving belongs in the application layer — never in the engine.

## Pull requests

1. Open an issue first for non-trivial changes (avoids wasted work).
2. Branch from `main`; keep PRs focused (one logical change).
3. Add or update a demo/test that exercises your change.
4. Ensure `PASS` on MSVC (and gcc/clang if you touch portable code).
5. Commits: imperative mood, explain *why*. By contributing you agree your
   contributions are licensed under the project's [MIT license](LICENSE).

## Spec changes → RFC

Format-level changes (the `.flux` wire layout, record fields, canonical facets,
composition semantics) go through the **RFC process** in [`docs/rfc/`](docs/rfc/).
A PR that changes the on-disk format MUST reference an accepted RFC and bump the
format version. Field additions are add-only (forward/backward compatible); field
removals require a deprecation cycle.

## Governance

FLUXmeme is MIT-licensed and currently maintainer-led; the roadmap is to move to
neutral, community governance under a foundation (Linux Foundation / ASWF) as the
ecosystem grows. Trademarks/naming: "FLUXmeme" and the `.flux`/`.fluxa` extensions
refer to this format; the spec is vendor-neutral.
