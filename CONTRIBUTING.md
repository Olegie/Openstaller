# Contributing To Openstaller

Openstaller accepts changes that preserve the central contract:

- generated installers remain human-readable
- package identity remains deterministic
- C owns orchestration and platform APIs
- Assembly owns byte-level identity kernels where native support exists
- unsupported platforms must retain the C fallback

## Development Loop

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Before proposing a change, generate at least one sample package:

```sh
build/openstaller-cli \
  --name "Openstaller Probe" \
  --source examples/sample_payload \
  --output dist \
  --install-dir "$HOME/.local/share/openstaller-probe"
```

## Code Style

The project is intentionally conservative C11:

- no hidden global allocator policy
- no external runtime dependencies for the core
- no stringly dynamic plugin system in the generator path
- no generated binary installer format until script emission is fully stable

Assembly changes must include a C fallback-compatible test expectation.
