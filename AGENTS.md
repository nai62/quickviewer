# QuickViewer agent instructions

## Scope and working principles

These instructions apply to the entire repository.

- Keep user changes intact and make narrowly scoped edits.
- Do not commit generated build products, deployed Qt DLLs, or local test
  reports.
- Preserve observable behavior during refactoring unless a behavior change is
  explicitly requested.
- Prefer ownership and state invariants that are explicit in types. Do not
  replace a raw pointer with a smart pointer until its actual ownership and
  QObject thread affinity are understood.
- Add a regression test before or with a crash fix whenever the affected layer
  can be exercised deterministically.

## Commit messages

- Use Conventional Commits-style subjects in the form `<type>: <description>`
  (for example, `fix: omit version from window titles`). Choose a type that
  describes the change, such as `fix`, `feat`, `docs`, `refactor`, or `chore`.

## Verification and handoff contract

Follow the repository `developer/README.md` documentation index; the Windows
build and test commands are in `developer/Testing.md`.

- Do not proactively run builds, automated tests, linters, benchmarks,
  deployment commands, or interactive checks unless the user explicitly
  requests their execution. Repository-configured commit hooks may run
  automatically as part of a user-requested commit operation.
- Do not bypass repository-configured commit hooks with `--no-verify` unless the
  user explicitly requests it.
- The tracked pre-commit hook formats staged first-party C++ files and stages
  those edits, so a commit can contain content that was not in the staged diff.
  Confirm with `git show` when the exact committed content matters.
- Stage whole first-party C++ files. The hook refuses a staged file that also
  has unstaged changes, so a partial (hunk-level) commit of such a file is not
  available; split commits by file instead.
- This execution policy does not relax test-coverage requirements. Logic or
  file-loading changes require the relevant automated tests to be added or
  updated, and crash fixes require the regression test described above whenever
  the affected layer can be exercised deterministically.
- Do not add tests for static specifications - constants, fixed sizes, and the
  metrics or wording a widget happens to have under the current style. A test
  that restates a constant pins it without checking any behaviour of ours, and
  it fails on the machine whose figures differ instead of finding a defect. Test
  what the code does with a value, not the value itself.
- At handoff, separately report builds, automated tests, and interactive checks
  as not run when applicable, and provide the narrowest appropriate commands or
  manual checks from the runbook.
- For project/build-system or broadly shared changes, recommend a targeted
  compile and link appropriate to the change. Recommend complete Debug or
  Release verification only when the change warrants it or the user requests it.
- For startup painting, fullscreen, OpenGL, input timing, and other GUI behavior,
  provide the relevant automated-test command and the required interactive
  Windows checks.
- When the user requests verification, never hide warnings or failures and
  distinguish pre-existing failures from failures introduced by the current
  diff. Do not claim that all tests passed if an expected executable was missing
  or skipped. Use process exit codes and the current test output, not historical
  fixed test counts.
- Report the exact failing QtTest function and data row when available.

When task-specific user instructions conflict with this file, follow the user.
