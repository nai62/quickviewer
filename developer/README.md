# Developer documentation

Entry point for working on QuickViewer. The published website lives in `docs/`;
this directory is for contributors.

## Start here

1. Set up the toolchain (Qt, Rust, submodules): see [HowToBuild.md](HowToBuild.md).
2. Build and test on Windows with `scripts\verify-windows.cmd debug`: see [Testing.md](Testing.md).
3. Enable the commit hook once per clone with `git config core.hooksPath .githooks`: see [CppLint.md](CppLint.md).

A typical loop is: edit, `scripts\verify-windows.cmd debug --build-viewer-only`,
run `scripts\verify-windows.cmd debug --test <name>`, then commit.

## Reference

| Document | Contents |
| --- | --- |
| [HowToBuild.md](HowToBuild.md) | Toolchain setup, HEIC/HEIF support, other build targets |
| [Testing.md](Testing.md) | Build and test commands, environment variables, verification policy, WSL, traps when adding sources or tests |
| [CppLint.md](CppLint.md) | clang-format scope, commands, pre-commit hook |
| [Architecture.md](Architecture.md) | Domain terminology and responsibility boundaries |
| [Benchmark.md](Benchmark.md) | Benchmark suites and measurement conditions |
