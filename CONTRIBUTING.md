# Contributing to Memogent

Thanks for taking the time. Memogent is built to last past the hackathon, so
we take code quality seriously from day one.

## Ground rules

- Every public header gets a doc comment.
- Every new module has at least one unit test (Catch2 in C++, pytest in Python).
- Every change that affects a Samsung KPI updates `docs/KPI_HARNESS.md` and
  the harness itself.
- No external network calls inside the core. `localhost:11434` (Ollama) is
  the sole exception, and only when a host app explicitly opts in.

## Dev environment

```bash
# C++ core
mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && cmake --build . -j

# Python reference
cd python && make dev

# iOS
cd bindings/ios && swift test

# Android
cd bindings/android && ./gradlew test
```

## Coding style

- C++ — clang-format (`.clang-format`), enforced by CI.
- Python — ruff, 100-col lines, enforced by CI.
- Swift — `swift-format` defaults.
- Kotlin — ktlint defaults.

## Commit messages

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <summary>

<body>
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `perf`, `build`, `ci`, `chore`.

## Pull requests

1. Open the PR against `main`.
2. Fill in the template. Skipped a test? Explain why.
3. CI must be green before review.
4. Squash-merge is the default.

## Running tests locally

```bash
cmake --build build --target memogent_tests && ./build/cpp_tests/memogent_tests
cd python && make test
cd python && make bench   # exits non-zero if any Samsung KPI target fails
```

## Security disclosures

See [`SECURITY.md`](SECURITY.md).
